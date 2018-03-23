/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2018 Dan Elkouby <streetwalkermc@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdint.h>
#include <string.h>
#include <lib.h>
#include <timestamp.h>
#include <arch/byteorder.h>
#include <arch/io.h>
#include <device/pci_def.h>
#include <device/pnp_def.h>
#include <cpu/x86/lapic.h>
#include <arch/acpi.h>
#include <console/console.h>
#include "northbridge/intel/sandybridge/sandybridge.h"
#include "northbridge/intel/sandybridge/raminit_native.h"
#include "southbridge/intel/bd82x6x/pch.h"
#include <southbridge/intel/common/gpio.h>
#include <arch/cpu.h>
#include <cpu/x86/msr.h>
#include <superio/nuvoton/common/nuvoton.h>
#include <superio/nuvoton/nct6779d/nct6779d.h>

#define SIO_PORT	0x2e
#define SIO_DEV		PNP_DEV(SIO_PORT, 0)
#define GPIO_PPOD_DEV	PNP_DEV(SIO_PORT, NCT6779D_GPIO_PP_OD)
#define ACPI_DEV	PNP_DEV(SIO_PORT, NCT6779D_ACPI)
#define HWM_DEV		PNP_DEV(SIO_PORT, NCT6779D_HWM_FPLED)
#define GPIO_DEV	PNP_DEV(SIO_PORT, NCT6779D_GPIO12345678_V)
#define GPIO01_DEV	PNP_DEV(SIO_PORT, NCT6779D_WDT1_GPIO01_V)

void pch_enable_lpc(void)
{
	pci_write_config16(PCH_LPC_DEV, LPC_EN, CNF1_LPC_EN | KBC_LPC_EN);
}

void mainboard_rcba_config(void)
{
}

const struct southbridge_usb_port mainboard_usb_ports[] = {
	{ 1, 2, 0 },
	{ 1, 2, 0 },
	{ 1, 2, 1 },
	{ 1, 2, 1 },
	{ 1, 2, 2 },
	{ 1, 2, 2 },
	{ 1, 2, 3 },
	{ 1, 2, 3 },
	{ 1, 2, 4 },
	{ 1, 2, 4 },
	{ 1, 2, 6 },
	{ 1, 2, 5 },
	{ 1, 2, 5 },
	{ 1, 2, 6 },
};

void mainboard_early_init(int s3resume)
{
}

void mainboard_config_superio(void)
{
	nuvoton_pnp_enter_conf_state(SIO_DEV);

	/* Pin function selection */
	pnp_write_config(SIO_DEV, 0x1a, 0x00);
	pnp_write_config(SIO_DEV, 0x2a, 0x40);
	pnp_write_config(SIO_DEV, 0x2c, 0x00);

	pnp_set_logical_device(GPIO_PPOD_DEV);
	pnp_write_config(SIO_DEV, 0xe4, 0xfc);
	pnp_write_config(SIO_DEV, 0xe6, 0x7f);

	pnp_set_logical_device(ACPI_DEV);
	pnp_write_config(SIO_DEV, 0xe2, 0x76);

	/* Power RAM in S3 */
	pnp_write_config(SIO_DEV, 0xe4, 0x10);

	pnp_set_logical_device(HWM_DEV);
	pnp_write_config(SIO_DEV, 0xe2, 0x7f);
	pnp_write_config(SIO_DEV, 0xe4, 0xf1);
	pnp_write_config(SIO_DEV, 0xf0, 0x3e);

	pnp_set_logical_device(GPIO_DEV);
	pnp_write_config(SIO_DEV, 0x30, 0x2e);

	/* GPIO2 */
	pnp_write_config(SIO_DEV, 0xe0, 0xdf);
	pnp_write_config(SIO_DEV, 0xe2, 0x00);
	pnp_write_config(SIO_DEV, 0xe9, 0x00);
	pnp_write_config(SIO_DEV, 0xe1, 0xc0);

	/* GPIO3 */
	pnp_write_config(SIO_DEV, 0xe4, 0x7f);
	pnp_write_config(SIO_DEV, 0xe6, 0x00);
	pnp_write_config(SIO_DEV, 0xea, 0x00);
	pnp_write_config(SIO_DEV, 0xe5, 0x70);

	/* GPIO5 */
	pnp_write_config(SIO_DEV, 0xf4, 0xfc);
	pnp_write_config(SIO_DEV, 0xf6, 0x00);
	pnp_write_config(SIO_DEV, 0xeb, 0x00);
	pnp_write_config(SIO_DEV, 0xf5, 0x88);

	pnp_set_logical_device(GPIO01_DEV);

	/* GPIO1 */
	pnp_write_config(SIO_DEV, 0xf0, 0x7f);
	pnp_write_config(SIO_DEV, 0xf2, 0x00);
	pnp_write_config(SIO_DEV, 0xf4, 0x00);
	pnp_write_config(SIO_DEV, 0xf1, 0x03);

	nuvoton_pnp_exit_conf_state(SIO_DEV);
}

void mainboard_get_spd(spd_raw_data *spd, bool id_only)
{
	read_spd(&spd[0], 0x50, id_only);
	read_spd(&spd[1], 0x51, id_only);
	read_spd(&spd[2], 0x52, id_only);
	read_spd(&spd[3], 0x53, id_only);
}
