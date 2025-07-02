/*
 * Copyright (C) 2024, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>

#ifdef CONFIG_AHAB_BOOT
#include "ahab.h"
#endif
#ifdef CONFIG_IMX_HAB
#include "hab.h"
#endif
#include "encryption.h"

int is_uboot_encrypted(void)
{
	char dek_blob[MAX_DEK_BLOB_SIZE];

	/* U-Boot is encrypted if and only if get_dek_blob does not fail */
	return !get_dek_blob((ulong)dek_blob, NULL);
}
