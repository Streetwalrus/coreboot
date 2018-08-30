/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2017-2018 Patrick Rudolph <siro@das-labor.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of
 * the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pciexp.h>
#include <device/pci_ids.h>
#include <assert.h>
#include <delay.h>

#include "sandybridge.h"

static void pcie_linktraining(device_t dev)
{
	size_t timeout = 100;
	u16 tmp;

	/* Start link training */
	tmp = pci_read_config16(dev, LCTL);
	pci_write_config16(dev, LCTL, tmp | (1 << 5));

	/* Wait for up to 100msec for link training to complete */
	while (timeout-- && (pci_read_config16(dev, LSTS) & (1 << 11)))
		udelay(1000);
}

static void pcie_change_link_speed(device_t dev, size_t speed)
{
	u16 tmp;

	/* Set new link speed */
	tmp = pci_read_config16(dev, LCTL2) & ~0xf;
	pci_write_config16(dev, LCTL2, tmp | speed);

	pcie_linktraining(dev);
}

static void pcie_programm_da0(device_t dev, u8 val)
{
	u32 tmp;

	val &= 0xf;

	tmp = pci_read_config32(dev, 0xd98);
	pci_write_config32(dev, 0xd98, tmp | 1);

	tmp = pci_read_config32(dev, 0xda0) & ~((0xf << 24) | (0xf << 8));
	pci_write_config32(dev, 0xda0, tmp | (val << 24) | (val << 8));
}

static bool pcie_get_lane_error(device_t dev, u8 lane)
{
	u32 tmp = pci_read_config32(dev, 0xe0c + lane * 0x10);

	/* Clear errors */
	pci_write_config32(dev, 0xe0c + lane * 0x10, tmp | (0xf << 16));

	return !!((tmp >> 16) & 0xf);
}

static bool pcie_get_port_error(device_t dev, u8 bundle)
{
	/* Lane has errors ? */
	if (pcie_get_lane_error(dev, bundle * 2 + 0) ||
	    pcie_get_lane_error(dev, bundle * 2 + 1)) {
		/* Clear error registers */
		pci_write_config16(dev, 0x0aa, 0xffff);
		pci_write_config16(dev, 0xd9c, 0xffff);
		pci_write_config16(dev, 0x1d0, 0xffff);
		pci_write_config32(dev, 0x1c4, 0xffffffff);
		pci_write_config32(dev, 0xc4a, 0xffffffff);

		return true;
	}

	/* Running at PCIe Gen3 ? */
	if ((pci_read_config16(dev, LSTS) & 0xf) != 3)
		return true;

	return false;
}

static void pcie_programm_90x(device_t dev, u8 bundle, u8 a, u8 b)
{
	u32 tmp;

	tmp = pci_read_config32(dev, 0x900 + bundle * 0x20) & ~(0xff << 10);
	pci_write_config32(dev, 0x900 + bundle * 0x20, tmp | (a << 10));

	tmp = pci_read_config32(dev, 0x90c + bundle * 0x20) & ~(0x3f << 11);
	pci_write_config32(dev, 0x90c + bundle * 0x20, tmp | (b << 11));
}

static size_t pcie_training1(device_t dev, u8 bundle)
{
	u32 tmp;
	size_t i;

	static const u8 a[] = {0, 0, 0, 0, 65, 97, 128, 129, 161, 192, 193,
			225, 225, 225, 225, 225};

	/* Set bit 20 in lane register 0xa00 */
	tmp = pci_read_config32(dev, 0xa00 + bundle * 0x10);
	pci_write_config32(dev, 0xa00 + bundle * 0x10, tmp | (1 << 20));

	tmp = pci_read_config32(dev, 0xa00 + (bundle + 1) * 0x10);
	pci_write_config32(dev, 0xa00 + (bundle + 1) * 0x10, tmp | (1 << 20));

	for (i = 15; i >= 4; i--) {
		u8 timeout = 50;
		/* Clear error register */
		pcie_get_port_error(dev, bundle);

		/* Programm test values */
		pcie_programm_90x(dev, bundle, a[i], i);

		/* Wait a little */
		mdelay(50);

		/* Now poll for errors */
		while (timeout-- && !pcie_get_port_error(dev, bundle))
			mdelay(1);

		/* No errors occured after 100msec ? */
		if (timeout == 0) {
			/* done */
			break;
		}

		/* Running at PCIe Gen3 ? */
		if ((pci_read_config16(dev, LSTS) & 0xf) != 3) {
			/* Error. Link has been lost */
			i = 0;
			break;
		}
	}

	/* Clear bit 20 in lane register 0xa00 */
	tmp = pci_read_config32(dev, 0xa00 + bundle * 0x10) & ~(1 << 20);
	pci_write_config32(dev, 0xa00 + bundle * 0x10, tmp);

	tmp = pci_read_config32(dev, 0xa00 + (bundle + 1) * 0x10) & ~(1 << 20);
	pci_write_config32(dev, 0xa00 + (bundle + 1) * 0x10, tmp);

	return i;
}

static void pcie_gen3_training(device_t dev)
{
	size_t bundle, start_bundle, end_bundle;
	static const u8 reg_da0[] = {7, 3, 5};

	printk(BIOS_DEBUG, "%s: Running PCIe Gen3 training\n", dev_path(dev));

	end_bundle = ((pci_read_config16(dev, LSTS) >> 4) & 0x3f) >> 1;

	start_bundle = 0;
	if (PCI_FUNC(dev->path.pci.devfn) == 1)
		start_bundle = 4;
	if (PCI_FUNC(dev->path.pci.devfn) == 2)
		start_bundle = 6;
	end_bundle += start_bundle;

	printk(BIOS_DEBUG, "%s: Training bundles %zu to %zu\n",
	       dev_path(dev), start_bundle, end_bundle - 1);

	for (bundle = start_bundle; bundle < end_bundle; bundle ++) {
		size_t result1[ARRAY_SIZE(reg_da0)] = {0};
		size_t best_da0 = 0;

		printk(BIOS_SPEW, "%s: Training bundle %zu\n",
		       dev_path(dev), bundle);

		for (size_t i = 0; i < ARRAY_SIZE(reg_da0); i++) {
			printk(BIOS_SPEW, "%s: 0xda0 = %u\n",
			       dev_path(dev), reg_da0[i]);

			/* Set PCIe Gen1 */
			pcie_change_link_speed(dev, 1);
			/* Programm 0xda0 */
			pcie_programm_da0(dev, reg_da0[i]);
			/* Set PCIe Gen3 */
			pcie_change_link_speed(dev, 3);
			/* Running at PCIe Gen3 ? */
			if ((pci_read_config16(dev, LSTS) & 0xf) != 3)
				continue;

			for (size_t j = 0; j < 4; j++) {
				/* Run training sequence 1 */
				size_t res = pcie_training1(dev, bundle);
				result1[i] += res;

				/* Retrain link in case it was lost */
				if (res == 0) {
					/* Set PCIe Gen1 */
					pcie_change_link_speed(dev, 1);
					/* Set PCIe Gen3 */
					pcie_change_link_speed(dev, 3);
				}
			}
			printk(BIOS_SPEW, "%s: training1 = %zu\n",
			       dev_path(dev), result1[i]);

			// FIXME: implement training2
		}
		size_t max = 0;
		for (size_t i = 0; i < ARRAY_SIZE(reg_da0); i++) {
			if (result1[i] > max) {
				max = result1[i];
				best_da0 = i;
			}
		}

		/* Set PCIe Gen1 */
		pcie_change_link_speed(dev, 1);

		if (max == 0) {
			printk(BIOS_DEBUG, "%s: Training failed\n", dev_path(dev));
			return;
		}

		printk(BIOS_DEBUG, "%s: Using best 0xda0 = %u on bundle %zu\n",
		       dev_path(dev), reg_da0[best_da0], bundle);

		/* Programm 0xda0 */
		pcie_programm_da0(dev, reg_da0[best_da0]);
		/* Set PCIe Gen3 */
		pcie_change_link_speed(dev, 3);
	}
}

static const bool pcie_dev_is_gen3_capable(device_t dev)
{
	const int cap = pci_find_capability(dev, PCI_CAP_ID_PCIE);

	if (!cap)
		return false;

	return (pci_read_config8(dev, cap + PCI_EXP_LNKCAP) & 0xf) > 2;
}

static void pcie_disable(struct device *dev)
{
	printk(BIOS_INFO, "%s: Disabling device\n", dev_path(dev));
	dev->enabled = 0;
}

static void pcie_init(device_t dev)
{
	struct bus *link;
	device_t child;

	/*
	 * PCIe Gen3 on IvyBridge+ needs a special startup sequence.
	 * As the MRC has its own initialization code skip it.
	 */
	if (((pci_read_config16(dev_find_slot(0, 0), PCI_DEVICE_ID) &
			BASE_REV_MASK) != BASE_REV_IVB) ||
		IS_ENABLED(CONFIG_HAVE_MRC))
		return;

	if ((pci_read_config16(dev, LCAP) & 0xf) != 3) {
		printk(BIOS_INFO, "%s: Limited to PCIe Gen2\n",
		       dev_path(dev));
		return;
	}

	if (!dev_is_active_bridge(dev)) {
		printk(BIOS_INFO, "%s: No children found\n",
		       dev_path(dev));
		return;
	}

	bool gen3_capable = true;
	for (link = dev->link_list; link; link = link->next) {
		for (child = link->children; child; child = child->sibling) {
			if (!child->enabled)
				continue;

			if (!pcie_dev_is_gen3_capable(child)) {
				printk(BIOS_INFO,
				       "%s: Child '%s' is not Gen3 capable\n",
				       dev_path(dev), dev_path(child));
				gen3_capable = false;
				break;
			}
		}
	}

	if (gen3_capable)
		pcie_gen3_training(dev);
}

#if IS_ENABLED(CONFIG_HAVE_ACPI_TABLES)
static const char *pcie_acpi_name(const struct device *dev)
{
	assert(dev);

	if (dev->path.type != DEVICE_PATH_PCI)
		return NULL;

	assert(dev->bus);
	if (dev->bus->secondary == 0)
		switch (dev->path.pci.devfn) {
		case PCI_DEVFN(1, 0):
			return "PEGP";
		case PCI_DEVFN(1, 1):
			return "PEG1";
		case PCI_DEVFN(1, 2):
			return "PEG2";
		case PCI_DEVFN(6, 0):
			return "PEG6";
		};

	struct device *const port = dev->bus->dev;
	assert(port);
	assert(port->bus);

	if (dev->path.pci.devfn == PCI_DEVFN(0, 0) &&
	    port->bus->secondary == 0 &&
	    (port->path.pci.devfn == PCI_DEVFN(1, 0) ||
	    port->path.pci.devfn == PCI_DEVFN(1, 1) ||
	    port->path.pci.devfn == PCI_DEVFN(1, 2) ||
	    port->path.pci.devfn == PCI_DEVFN(6, 0)))
		return "DEV0";

	return NULL;
}
#endif

static void
pcie_set_subsystem(struct device *dev, unsigned int ven, unsigned int device)
{
	/* NOTE: This is not the default position! */
	if (!ven || !device)
		pci_write_config32(dev, 0x94,
				   pci_read_config32(dev, 0));
	else
		pci_write_config32(dev, 0x94,
				   ((device & 0xffff) << 16) | (ven & 0xffff));
}

static struct pci_operations pci_ops = {
	.set_subsystem = pcie_set_subsystem,
};

static struct device_operations device_ops = {
	.read_resources		= pci_bus_read_resources,
	.set_resources		= pci_dev_set_resources,
	.enable_resources	= pci_bus_enable_resources,
	.scan_bus		= pciexp_scan_bridge,
	.reset_bus		= pci_bus_reset,
	.disable		= pcie_disable,
	.init			= pcie_init,
	.ops_pci		= &pci_ops,
#if IS_ENABLED(CONFIG_HAVE_ACPI_TABLES)
	.acpi_name		= pcie_acpi_name,
#endif
};

static const unsigned short pci_device_ids[] = { 0x0101, 0x0105, 0x0109, 0x010d,
						 0x0151, 0x0155, 0x0159, 0x015d,
						 0 };

static const struct pci_driver pch_pcie __pci_driver = {
	.ops		= &device_ops,
	.vendor		= PCI_VENDOR_ID_INTEL,
	.devices	= pci_device_ids,
};
