/*
 * Copyright (C) 2025, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef TF_ENCRYPTION_H
#define TF_ENCRYPTION_H

#include <fsl_sec.h>

/*
 * DEK header size is always 8 regardless of the backend implementation:
 * WRP_HDR_SIZE, OPTEE_BLOB_HDR_SIZE, DEK_BLOB_HDR_SIZE.
 */
#define DEK_BLOB_SIZE(x)	(BLOB_SIZE(x) + 8)

/* Maximum key size supported: 256 bits (32 bytes) */
#define MAX_DEK_BLOB_SIZE	DEK_BLOB_SIZE(32)

int is_uboot_encrypted(void);

#endif /* TF_ENCRYPTION_H */
