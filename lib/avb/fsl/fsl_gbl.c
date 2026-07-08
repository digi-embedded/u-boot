/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * Copyright 2026 NXP
 *
 */

#include <image.h>
#include <log.h>
#include <string.h>
#include <malloc.h>
#include <mmc.h>
#include <fs.h>
#include <u-boot/sha256.h>
#include <u-boot/rsa.h>
#include <trusty/libtipc.h>
#include "fsl_gbl.h"
#include "../lib/avb/fsl/utils.h"

int load_file_from_android_esp(char *part_name, const char *file_path,
			       uint8_t **buf_out, loff_t *size_out)
{
	char dev_part_str[32];
	int dev_no;
	loff_t file_size = 0;
	loff_t actread = 0;
	uint8_t *buf = NULL;
	int ret;

	dev_no = mmc_get_env_dev();
	snprintf(dev_part_str, sizeof(dev_part_str), "%d#%s", dev_no, part_name);

	ret = fs_set_blk_dev("mmc", dev_part_str, FS_TYPE_FAT);
	if (ret != 0) {
		printf("Failed to mount FAT32 on %s partition! ret=%d\n", part_name, ret);
		return -1;
	}

	ret = fs_size(file_path, &file_size);
	if (ret != 0 || file_size == 0) {
		printf("Failed to get size of %s (ret=%d, size=%lld)\n",
		       file_path, ret, file_size);
		fs_close();
		return -1;
	}

	buf = memalign(ALIGN_BYTES, file_size);
	if (buf == NULL) {
		printf("Failed to allocate %lld bytes for %s!\n", file_size, file_path);
		fs_close();
		return -1;
	}

	ret = fs_set_blk_dev("mmc", dev_part_str, FS_TYPE_FAT);
	if (ret != 0) {
		printf("Failed to re-mount FAT32 filesystem!\n");
		free(buf);
		return -1;
	}

	ret = fs_read(file_path, (ulong)buf, 0, file_size, &actread);
	fs_close();

	if (ret != 0 || actread != file_size) {
		printf("Failed to read %s! ret=%d, read=%lld, expected=%lld\n",
		       file_path, ret, actread, file_size);
		free(buf);
		return -1;
	}

	*buf_out = buf;
	*size_out = file_size;

	return 0;
}

#ifdef CONFIG_IMX_TRUSTY_OS
extern int is_current_slot_successful(bool *success);

int verify_gbl_metadata(struct gbl_metadata *metadata) {
	int ret = 0;
	uint64_t stored_rbindex = 0;

	assert(metadata);

	/* Check the magic */
	if (memcmp(metadata->magic, GBL0_MAGIC, sizeof(GBL0_MAGIC))) {
		printf("Invalid gbl metadata magic!\n");
		return -1;
	}

	/* Verify rollback index */
	if (metadata->rollback_index_location >= ROLLBACK_INDEX_SLOT_MAX) {
		printf("Invalid gbl rollback index location!\n");
		return -1;
	}
	ret = trusty_read_rollback_index(metadata->rollback_index_location,
					 &stored_rbindex);
	if (ret != 0) {
		printf("Failed to read gbl rollback index!\n");
		return -1;
	}
	if (metadata->rollback_index < stored_rbindex) {
		printf("GBL rollback index rejected!\n");
		return -1;
	}

	/* Update the stored rollback index if applied */
	if (metadata->rollback_index > stored_rbindex) {
		bool success_boot = false;
		if (is_current_slot_successful(&success_boot) < 0) {
			return -1;
		}
		/* Update the rollback index when current
		 * slot is successfully booted.
		 */
		if (success_boot) {
			ret = trusty_write_rollback_index(metadata->rollback_index_location,
							  metadata->rollback_index);
			if (ret != 0) {
				printf("Failed to write gbl rollback index!\n");
				return -1;
			}
		}
	}

	return 0;
}

int verify_gbl_signature(uint8_t *gbl, uint32_t gbl_size,
			 uint8_t *metadata, uint32_t metadata_size,
			 uint8_t *signature, uint32_t sig_size) {
	uint8_t hash[SHA256_SUM_LEN];
	uint8_t public_key_buf[2048];
	uint32_t public_key_sz = 0;
	struct image_sign_info info;
	sha256_context ctx;
	char algo[64];
	int ret = 0;

	assert(gbl);
	assert(metadata);
	assert(signature);

	if (sig_size != RSA4096_SIG_LEN) {
		printf("Wrong gbl signature size! expected %d, got %d\n",
		       RSA4096_SIG_LEN, sig_size);
		return -1;
	}

	/* Calculate the hash of signed data: gbl image + metadata */
	sha256_starts(&ctx);
	sha256_update(&ctx, gbl, gbl_size);
	sha256_update(&ctx, metadata, metadata_size);
	sha256_finish(&ctx, hash);

	/* Load GBL public key from secure storage */
	public_key_sz = sizeof(public_key_buf);
	if (trusty_read_gbl_public_key(public_key_buf,
				       &public_key_sz) != 0) {
		printf("Failed to read gbl public key!\n");
		return -1;
	}

	/* Verify the signature, only support SHA256_RSA4096 algorithm */
	memset(&info, '\0', sizeof(info));
	memset(algo, 0, sizeof(algo));

	info.padding = image_get_padding_algo("pkcs-1.5");
	memcpy(algo, "sha256,rsa4096", sizeof("sha256,rsa4096"));
	info.checksum = image_get_checksum_algo(algo);
	info.name = (const char *)algo;
	info.crypto = image_get_crypto_algo(info.name);
	if (!info.checksum || !info.crypto) {
		printf("<%s> not supported on image_get_(checksum|crypto)_algo()\n", algo);
		return -1;
	}

	info.key = public_key_buf;
	info.keylen = public_key_sz;

	ret = rsa_verify_with_pkey(&info, hash, signature, RSA4096_SIG_LEN);
	if (ret != 0) {
		printf("GBL signature verify failed! err: %d\n", ret);
		return -1;
	}

	return 0;
}

int verify_gbl(uint8_t *gbl, uint32_t gbl_size,
	       uint8_t *metadata_buf, uint32_t metadata_buf_size) {
	gbl_metadata *metadata = NULL;
	uint8_t *signature = NULL;

	assert(gbl);
	assert(metadata_buf);

	if (metadata_buf_size != sizeof(gbl_metadata) + RSA4096_SIG_LEN) {
		printf("Invalid gbl metadata file size! expected %lu, got %d\n",
		       sizeof(gbl_metadata) + RSA4096_SIG_LEN, metadata_buf_size);
		return -1;
	}

	metadata = (gbl_metadata *)metadata_buf;
	signature = metadata_buf + sizeof(gbl_metadata);

	if (metadata->original_gbl_size != gbl_size) {
		printf("GBL size mismatch! metadata: %d, actual: %d\n",
		       metadata->original_gbl_size, gbl_size);
		return -1;
	}

	/* Verify the signature */
	if (verify_gbl_signature(gbl, gbl_size,
				 (uint8_t *)metadata, sizeof(gbl_metadata),
				 signature, RSA4096_SIG_LEN))
		return -1;

	/* Verify the metadata */
	if (verify_gbl_metadata(metadata))
		return -1;

	return 0;
}
#endif /* CONFIG_IMX_TRUSTY_OS */
