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

#include <southbridge/intel/bd82x6x/nvs.h>

/* FIXME: check this function.  */
void acpi_create_gnvs(global_nvs_t *gnvs)
{
	/* Disable USB ports in S3 by default */
	gnvs->s3u0 = 0;
	gnvs->s3u1 = 0;

	/* Disable USB ports in S5 by default */
	gnvs->s5u0 = 0;
	gnvs->s5u1 = 0;

	// the lid is open by default.
	gnvs->lids = 1;

	gnvs->tcrt = 100;
	gnvs->tpsv = 90;
}
