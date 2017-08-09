/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2007-2010 coresystems GmbH
 * Copyright (C) 2011 Google Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <console/console.h>
#include <arch/io.h>
#include <arch/acpi.h>
#include <device/pci_def.h>
#include <elog.h>
#include <cbmem.h>
#include <pc80/mc146818rtc.h>
#include <romstage_handoff.h>
#include "sandybridge.h"

static void sandybridge_setup_bars(void)
{
	/* Setting up Southbridge. In the northbridge code. */
	printk(BIOS_DEBUG, "Setting up static southbridge registers...");
	pci_write_config32(PCH_LPC_DEV, RCBA, (uintptr_t)DEFAULT_RCBA | 1);

	pci_write_config32(PCH_LPC_DEV, PMBASE, DEFAULT_PMBASE | 1);
	pci_write_config8(PCH_LPC_DEV, ACPI_CNTL, 0x80); /* Enable ACPI BAR */

	printk(BIOS_DEBUG, " done.\n");

	printk(BIOS_DEBUG, "Disabling Watchdog reboot...");
	RCBA32(GCS) = RCBA32(GCS) | (1 << 5);	/* No reset */
	outw((1 << 11), DEFAULT_PMBASE | 0x60 | 0x08);	/* halt timer */
	printk(BIOS_DEBUG, " done.\n");

	printk(BIOS_DEBUG, "Setting up static northbridge registers...");
	/* Set up all hardcoded northbridge BARs */
	pci_write_config32(PCI_DEV(0, 0x00, 0), EPBAR, DEFAULT_EPBAR | 1);
	pci_write_config32(PCI_DEV(0, 0x00, 0), EPBAR + 4, (0LL+DEFAULT_EPBAR) >> 32);
	pci_write_config32(PCI_DEV(0, 0x00, 0), MCHBAR, (uintptr_t)DEFAULT_MCHBAR | 1);
	pci_write_config32(PCI_DEV(0, 0x00, 0), MCHBAR + 4, (0LL+(uintptr_t)DEFAULT_MCHBAR) >> 32);
	pci_write_config32(PCI_DEV(0, 0x00, 0), DMIBAR, (uintptr_t)DEFAULT_DMIBAR | 1);
	pci_write_config32(PCI_DEV(0, 0x00, 0), DMIBAR + 4, (0LL+(uintptr_t)DEFAULT_DMIBAR) >> 32);

	/* Set C0000-FFFFF to access RAM on both reads and writes */
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM0, 0x30);
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM1, 0x33);
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM2, 0x33);
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM3, 0x33);
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM4, 0x33);
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM5, 0x33);
	pci_write_config8(PCI_DEV(0, 0x00, 0), PAM6, 0x33);

#if IS_ENABLED(CONFIG_ELOG_BOOT_COUNT)
	/* Increment Boot Counter for non-S3 resume */
	if ((inw(DEFAULT_PMBASE + PM1_STS) & WAK_STS) &&
	    ((inl(DEFAULT_PMBASE + PM1_CNT) >> 10) & 7) != SLP_TYP_S3)
		boot_count_increment();
#endif

	printk(BIOS_DEBUG, " done.\n");

#if IS_ENABLED(CONFIG_ELOG_BOOT_COUNT)
	/* Increment Boot Counter except when resuming from S3 */
	if ((inw(DEFAULT_PMBASE + PM1_STS) & WAK_STS) &&
	    ((inl(DEFAULT_PMBASE + PM1_CNT) >> 10) & 7) == SLP_TYP_S3)
		return;
	boot_count_increment();
#endif
}

static void sandybridge_setup_graphics(void)
{
	u32 reg32;
	u16 reg16;
	u8 reg8;
	u8 gfxsize;

	/* Just make sure IGD is on ... */
	reg32 = pci_read_config32(PCI_DEV(0, 0, 0), DEVEN) | DEVEN_IGD;
	pci_write_config32(PCI_DEV(0, 0, 0), DEVEN, reg32);

	reg16 = pci_read_config16(PCI_DEV(0,2,0), PCI_DEVICE_ID);
	switch (reg16) {
	case 0x0102: /* GT1 Desktop */
	case 0x0106: /* GT1 Mobile */
	case 0x010a: /* GT1 Server */
	case 0x0112: /* GT2 Desktop */
	case 0x0116: /* GT2 Mobile */
	case 0x0122: /* GT2 Desktop >=1.3GHz */
	case 0x0126: /* GT2 Mobile >=1.3GHz */
	case 0x0152: /* IvyBridge */
	case 0x0156: /* IvyBridge */
	case 0x0162: /* IvyBridge */
	case 0x0166: /* IvyBridge */
	case 0x016a: /* IvyBridge */
		break;
	default:
		printk(BIOS_DEBUG, "Graphics not supported by this CPU/chipset.\n");
		return;
	}

	printk(BIOS_DEBUG, "Initializing Graphics...\n");

	if (get_option(&gfxsize, "gfx_uma_size") != CB_SUCCESS) {
		/* Setup IGD memory by setting GGC[7:3] = 1 for 32MB */
		gfxsize = 0;
	}
	reg16 = pci_read_config16(PCI_DEV(0,0,0), GGC);
	reg16 &= ~0x00f8;
	reg16 |= (gfxsize + 1) << 3;
	/* Program GTT memory by setting GGC[9:8] = 2MB */
	reg16 &= ~0x0300;
	reg16 |= 2 << 8;
	/* Enable VGA decode */
	reg16 &= ~0x0002;
	pci_write_config16(PCI_DEV(0,0,0), GGC, reg16);

	/* Enable 256MB aperture */
	reg8 = pci_read_config8(PCI_DEV(0, 2, 0), MSAC);
	reg8 &= ~0x06;
	reg8 |= 0x02;
	pci_write_config8(PCI_DEV(0, 2, 0), MSAC, reg8);

	/* Erratum workarounds */
	reg32 = MCHBAR32(0x5f00);
	reg32 |= (1 << 9)|(1 << 10);
	MCHBAR32(0x5f00) = reg32;

	/* Enable SA Clock Gating */
	reg32 = MCHBAR32(0x5f00);
	MCHBAR32(0x5f00) = reg32 | 1;

	/* GPU RC6 workaround for sighting 366252 */
	reg32 = MCHBAR32(0x5d14);
	reg32 |= (1 << 31);
	MCHBAR32(0x5d14) = reg32;

	/* VLW */
	reg32 = MCHBAR32(0x6120);
	reg32 &= ~(1 << 0);
	MCHBAR32(0x6120) = reg32;

	reg32 = MCHBAR32(0x5418);
	reg32 |= (1 << 4) | (1 << 5);
	MCHBAR32(0x5418) = reg32;
}

/*
 * Configure the PEG port.
 * Split the PEG port depending on HWSTRAP.
 * Limit the maximum link speed depending on Gen3 fuse.
 */
static void peg1x_port_config(void)
{
	u32 deven;
	u8 peg_lanes[3] = {16, 0, 0};
	u8 link_speed = 3;

	printk(BIOS_DEBUG, "PEG: lane configuration ");
	switch ((pci_read_config32(PCI_DEV(0, 1, 0), HWSTRAP) >> 16) & 0x3) {
	case 0:
		printk(BIOS_DEBUG, "x8 x4 x4\n");
		peg_lanes[0] = 8;
		peg_lanes[1] = 4;
		peg_lanes[2] = 4;
		break;
	case 2:
		printk(BIOS_DEBUG, "x8 x8\n");
		peg_lanes[0] = 8;
		peg_lanes[1] = 8;
		peg_lanes[2] = 0;
		break;
	default:
		printk(BIOS_DEBUG, "x16\n");
		break;
	}

	/*
	 * Disable PCIe Gen3 on:
	 * * SandyBridge
	 * * CAPID0_B "PEG10 Gen3 feature disable fuse"
	 */
	if (((pci_read_config16(PCI_DEV(0, 0, 0), PCI_DEVICE_ID) &
	    BASE_REV_MASK) != BASE_REV_IVB) ||
	    (pci_read_config32(PCI_DEV(0, 0, 0), CAPID0_B) & (1 << 20))) {
		printk(BIOS_DEBUG, "PEG: PCIe Gen3 disabled\n");
		link_speed = 2;
	} else {
		printk(BIOS_DEBUG, "PEG: PCIe Gen3 supported\n");
	}

	/*
	 * The HWSTRAP configures DEVEN and sets the link width on each
	 * PEG port.
	 * Note that the LCAP bit's are read/write-once while documentation
	 * states that they are read-only!
	 */
	deven = pci_read_config16(PCI_DEV(0, 0, 0), DEVEN);
	deven &= ~(DEVEN_PEG10 | DEVEN_PEG11 | DEVEN_PEG12);
	if (peg_lanes[0] > 0) {
		deven |= DEVEN_PEG10;
		u32 tmp = pci_read_config32(PCI_DEV(0, 1, 0), LCAP);
		tmp &= ~((0x1f << 4) | (0xf << 0));
		tmp |= peg_lanes[0] << 4;
		tmp |= link_speed;
		pci_write_config32(PCI_DEV(0, 1, 0), LCAP, tmp);
	}
	if (peg_lanes[1] > 0) {
		deven |= DEVEN_PEG11;
		u32 tmp = pci_read_config32(PCI_DEV(0, 1, 1), LCAP);
		tmp &= ~((0x1f << 4) | (0xf << 0));
		tmp |= peg_lanes[1] << 4;
		tmp |= link_speed;
		pci_write_config32(PCI_DEV(0, 1, 1), LCAP, tmp);
	}
	if (peg_lanes[2] > 0) {
		deven |= DEVEN_PEG12;
		u32 tmp = pci_read_config32(PCI_DEV(0, 1, 2), LCAP);
		tmp &= ~((0x1f << 4) | (0xf << 0));
		tmp |= peg_lanes[2] << 4;
		tmp |= link_speed;
		pci_write_config32(PCI_DEV(0, 1, 2), LCAP, tmp);
	}
	pci_write_config16(PCI_DEV(0, 0, 0), DEVEN, deven);
}

/* Static PHY configuration for IvyBridge */
static void ivybridge_peg_phy(void)
{
	u32 tmp;

	struct pcie_config {
		u16 reg;
		u32 and_mask;
		u32 or_mask;
	};

	static const struct pcie_config lanes[] = {
		{0x0a00, 0xf0d9ffc1, 0x0324001a},
		{0x0a04, 0xfff9e7ff, 0x00021800},
	};
	static const struct pcie_config bundles[] = {
		{0x0700, 0x04177ff7, 0x09e88008},
		{0x0da0, 0x80F080F0, 0x27082708},
		{0x0900, 0xf3ffffff, 0x00000000},
		{0x0904, 0x8000f000, 0x2a1804a2},
		{0x0908, 0x27ffffff, 0x50000000},
		{0x090c, 0xc111f81f, 0x0ea00120},
		{0x0910, 0xffffc03f, 0x00000340},
		{0x0914, 0x77c21bff, 0x8815a400},
	};
	static const struct pcie_config single[] = {
		{0x0308, 0xff80ff00, 0x0012006c},
		{0x0314, 0xff80ffff, 0x00130000},
		{0x0dd8, 0xffffbfff, 0x00004000},
	};

	for (size_t j = 0; j < ARRAY_SIZE(lanes); j++)
		for (size_t i = 0; i < 16; i++) {
			const u16 reg = lanes[j].reg + i * 0x10;

			tmp = pci_read_config32(PCI_DEV(0, 1, 0), reg);
			tmp &= lanes[j].and_mask;
			tmp |= lanes[j].or_mask;
			pci_write_config32(PCI_DEV(0, 1, 0), reg, tmp);
		}

	for (size_t j = 0; j < ARRAY_SIZE(bundles); j++)
		for (size_t i = 0; i < 8; i++) {
			const u16 reg = bundles[j].reg + i * 0x20;

			tmp = pci_read_config32(PCI_DEV(0, 1, 0), reg);
			tmp &= bundles[j].and_mask;
			tmp |= bundles[j].or_mask;
			pci_write_config32(PCI_DEV(0, 1, 0), reg, tmp);
		}

	for (size_t j = 0; j < ARRAY_SIZE(single); j++) {
		tmp = pci_read_config32(PCI_DEV(0, 1, 0), single[j].reg);
		tmp &= single[j].and_mask;
		tmp |= single[j].or_mask;
		pci_write_config32(PCI_DEV(0, 1, 0), single[j].reg, tmp);
	}
}

static void start_peg_link_training(void)
{
	u32 tmp;
	u32 deven;

	/* PEG on IvyBridge+ needs a special startup sequence.
	 * As the MRC has its own initialization code skip it. */
	if (((pci_read_config16(PCI_DEV(0, 0, 0), PCI_DEVICE_ID) &
			BASE_REV_MASK) != BASE_REV_IVB) ||
		IS_ENABLED(CONFIG_HAVE_MRC))
		return;

	/* Configure PEG10/PEG11/PEG12 PHY */
	ivybridge_peg_phy();

	deven = pci_read_config32(PCI_DEV(0, 0, 0), DEVEN);

	if (deven & DEVEN_PEG10) {
		/* Run device detection at 2.5GT/s */
		tmp = pci_read_config32(PCI_DEV(0, 1, 0), LCTL2) & ~0xf;
		pci_write_config32(PCI_DEV(0, 1, 0), LCTL2, tmp | 1);

		/* Start link training */
		tmp = pci_read_config32(PCI_DEV(0, 1, 0), 0xC24) & ~(1 << 16);
		pci_write_config32(PCI_DEV(0, 1, 0), 0xC24, tmp | (1 << 5));
	}

	if (deven & DEVEN_PEG11) {
		/* Run device detection at 2.5GT/s */
		tmp = pci_read_config32(PCI_DEV(0, 1, 1), LCTL2) & ~0xf;
		pci_write_config32(PCI_DEV(0, 1, 1), LCTL2, tmp | 1);

		/* Start link training */
		tmp = pci_read_config32(PCI_DEV(0, 1, 1), 0xC24) & ~(1 << 16);
		pci_write_config32(PCI_DEV(0, 1, 1), 0xC24, tmp | (1 << 5));
	}

	if (deven & DEVEN_PEG12) {
		/* Run device detection at 2.5GT/s */
		tmp = pci_read_config32(PCI_DEV(0, 1, 2), LCTL2) & ~0xf;
		pci_write_config32(PCI_DEV(0, 1, 2), LCTL2, tmp | 1);

		/* Start link training */
		tmp = pci_read_config32(PCI_DEV(0, 1, 2), 0xC24) & ~(1 << 16);
		pci_write_config32(PCI_DEV(0, 1, 2), 0xC24, tmp | (1 << 5));
	}

	if (deven & DEVEN_PEG60) {
		tmp = pci_read_config32(PCI_DEV(0, 6, 0), 0xC24) & ~(1 << 16);
		pci_write_config32(PCI_DEV(0, 6, 0), 0xC24, tmp | (1 << 5));
	}
}

void sandybridge_early_initialization(void)
{
	u32 capid0_a;
	u8 reg8;

	/* Device ID Override Enable should be done very early */
	capid0_a = pci_read_config32(PCI_DEV(0, 0, 0), 0xe4);
	if (capid0_a & (1 << 10)) {
		const size_t is_mobile = get_platform_type() == PLATFORM_MOBILE;

		reg8 = pci_read_config8(PCI_DEV(0, 0, 0), 0xf3);
		reg8 &= ~7; /* Clear 2:0 */

		if (is_mobile)
			reg8 |= 1; /* Set bit 0 */

		pci_write_config8(PCI_DEV(0, 0, 0), 0xf3, reg8);
	}

	/* Setup all BARs required for early PCIe and raminit */
	sandybridge_setup_bars();

	/* Setup IOMMU BARs */
	sandybridge_init_iommu();

	sandybridge_setup_graphics();

	/* Read PEG HWSTRAP and configure ports */
	peg1x_port_config();

	/* Write magic value to start PEG link training.
	 * This should be done in PCI device enumeration, but
	 * the PCIe specification requires to wait at least 100msec
	 * after reset for devices to come up.
	 * As we don't want to increase boot time, enable it early and
	 * assume the PEG is up as soon as PCI enumeration starts.
	 * TODO: use time stamps to ensure the timings are met */
	start_peg_link_training();
}

void northbridge_romstage_finalize(int s3resume)
{
	MCHBAR16(SSKPD) = 0xCAFE;

	romstage_handoff_init(s3resume);
}
