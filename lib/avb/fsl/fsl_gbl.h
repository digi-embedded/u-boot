/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * Copyright 2026 NXP
 *
 */

#ifndef __FSL_GBL_H__
#define __FSL_GBL_H__

/* Magic values for validation */
#define GBL0_MAGIC "GBL0\0"

/* Fixed sizes and offsets */
#define RSA4096_SIG_LEN 512
#define ROLLBACK_INDEX_SLOT_MAX (32)

/* Path to GBL image file in android_esp partition */
#define GBL_EFI_PATH        "/EFI/BOOT/BOOTAA64.EFI"
/* Path to GBL metadata file in android_esp partition */
#define GBL_METADATA_PATH "/EFI/BOOT/gbl_metadata.bin"

typedef struct gbl_metadata {
	/* GBL0_MAGIC */
	char magic[8];
	/* Size of original GBL image */
	uint32_t original_gbl_size;
	/* Rollback index location */
	uint32_t rollback_index_location;
	/* Rollback index of the GBL image */
	uint32_t rollback_index;
} gbl_metadata;

int load_file_from_android_esp(char *part_name, const char *file_path,
			       uint8_t **buf_out, loff_t *size_out);
int verify_gbl_metadata(struct gbl_metadata *metadata);
int verify_gbl_signature(uint8_t *gbl, uint32_t gbl_size,
			 uint8_t *metadata, uint32_t metadata_size,
			 uint8_t *signature, uint32_t sig_size);
int verify_gbl(uint8_t *gbl, uint32_t gbl_size,
	       uint8_t *metadata_buf, uint32_t metadata_buf_size);
#endif
