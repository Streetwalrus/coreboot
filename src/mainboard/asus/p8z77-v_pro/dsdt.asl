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

#include <arch/acpi.h>
#define ACPI_VIDEO_DEVICE \_SB.PCI0.GFX0
DefinitionBlock(
	"dsdt.aml",
	"DSDT",
	0x02,           // DSDT Revision: ACPI v2.0 and up
        OEM_ID,
        ACPI_TABLE_CREATOR,
	0x20141018	// OEM revision
)
{
	// Some generic macros
	#include "acpi/platform.asl"
	#include <cpu/intel/common/acpi/cpu.asl>
	#include <southbridge/intel/bd82x6x/acpi/platform.asl>
	/* global NVS and variables.  */
	#include <southbridge/intel/bd82x6x/acpi/globalnvs.asl>
	#include <southbridge/intel/bd82x6x/acpi/sleepstates.asl>

	Scope (\_SB) {
		Device (PCI0)
		{
		#include <northbridge/intel/sandybridge/acpi/sandybridge.asl>
		#include <drivers/intel/gma/acpi/default_brightness_levels.asl>
		#include <southbridge/intel/bd82x6x/acpi/pch.asl>
		}
	}
}
