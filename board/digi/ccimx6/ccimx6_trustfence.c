/*
 * Copyright (C) 2024, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <fsl_sec.h>
#include <asm/mach-imx/hab.h>

#include "../common/trustfence.h"

#define UBOOT_HEADER_SIZE	0xC00
#define UBOOT_START_ADDR	(CONFIG_TEXT_BASE - UBOOT_HEADER_SIZE)

/*
 * If CONFIG_CSF_SIZE is undefined, assume 0x4000. This value will be used
 * in the signing script.
 */
#ifndef CONFIG_CSF_SIZE
#define CONFIG_CSF_SIZE 0x4000
#endif

/*
 * Return the offset where the DEK blob must be placed for:
 * - offset[0] -> SPL
 * - offset[1] -> U-Boot
 *
 * DEK blob will be directly appended to the U-Boot image.
 */
int get_dek_blob_offset(ulong addr, ulong size, u32 *offset)
{
	offset[0] = 0;	/* No SPL*/
	offset[1] = size;

	return 0;
}

/*
 * Copy the DEK blob used by the current U-Boot image into a buffer. Also
 * get its size in the last out parameter.
 * Possible DEK key sizes are 128, 192 and 256 bits.
 * DEK blobs have an overhead of 56 bytes.
 * Hence, possible DEK blob sizes are 72, 80 and 88 bytes.
 *
 * The output buffer should be at least MAX_DEK_BLOB_SIZE (88) bytes long to
 * prevent out of boundary access.
 *
 * Returns 0 if the DEK blob was found, 1 otherwise.
 */
int get_dek_blob(ulong addr, u32 *size)
{
	struct ivt *ivt = (struct ivt *)UBOOT_START_ADDR;

	/* Verify the pointer is pointing at an actual IVT table */
	if ((ivt->hdr.magic != IVT_HEADER_MAGIC) ||
	    (be16_to_cpu(ivt->hdr.length) != IVT_TOTAL_LENGTH))
		return 1;

	if (ivt->csf) {
		int blob_size = MAX_DEK_BLOB_SIZE;
		uint8_t *dek_blob = (uint8_t *)(uintptr_t)(ivt->csf +
				    CONFIG_CSF_SIZE - blob_size);

		/*
		 * Several DEK sizes can be used.
		 * Determine the size and the start of the DEK blob by looking
		 * for its header.
		 */
		while (*dek_blob != HDR_TAG && blob_size > 0) {
			dek_blob += 8;
			blob_size -= 8;
		}

		if (blob_size > 0) {
			if (size)
				*size = blob_size;
			memcpy((void *)addr, (void *)dek_blob, blob_size);
			return 0;
		}
	}

	return 1;
}
