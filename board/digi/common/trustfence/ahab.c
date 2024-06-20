/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <asm/mach-imx/image.h>
#include <linux/types.h>
#include <mmc.h>
#include <stdlib.h>

#include "ahab.h"

extern int mmc_get_bootdevindex(void);

/*
 * Get next AHAB container address after 'addr'
 */
static ulong get_next_container_addr(ulong addr)
{
	struct container_hdr *phdr;
	struct boot_img_t *img_entry;

	phdr = (struct container_hdr *)(addr);
	if (phdr->tag != AHAB_CNTR_HDR_TAG
	    || phdr->version != AHAB_CNTR_HDR_VER) {
		printf("%s: wrong base container header\n", __func__);
		return -1;
	}

	img_entry = (struct boot_img_t *)(addr + sizeof(struct container_hdr));
	img_entry += (phdr->num_images - 1);
	phdr =
	    (struct container_hdr *)ROUND(addr + img_entry->offset +
					  img_entry->size, SZ_1K);
	if (phdr->tag != AHAB_CNTR_HDR_TAG
	    || phdr->version != AHAB_CNTR_HDR_VER) {
		printf("%s: wrong next container header\n", __func__);
		return -1;
	}

	return (ulong) phdr;
}

/*
 * Get blob address from AHAB container in 'addr'
 */
static ulong get_blob_addr_from_container(ulong addr)
{
	struct container_hdr *phdr;
	struct signature_block_hdr *sign_hdr;
	ulong blob_addr;

	phdr = (struct container_hdr *)(addr);
	if (phdr->tag != AHAB_CNTR_HDR_TAG
	    || phdr->version != AHAB_CNTR_HDR_VER) {
		printf("Error: wrong container header\n");
		return -1;
	}
	debug("AHAB container header address = 0x%08lx\n", (ulong) phdr);

	sign_hdr =
	    (struct signature_block_hdr *)((ulong) phdr + phdr->sig_blk_offset);
	if (sign_hdr->tag != AHAB_SIGN_HDR_TAG
	    || sign_hdr->version != AHAB_SIGN_HDR_VER) {
		debug("Error: wrong signature block header\n");
		return -1;
	}
	debug("Signature block header address = 0x%08lx\n", (ulong) sign_hdr);

	blob_addr = (ulong) sign_hdr + sign_hdr->blob_offset;
	debug("Blob header address = 0x%08lx\n", blob_addr);

	return blob_addr;
}

int get_dek_blob_offset(ulong addr, u32 *offset)
{
	ulong container_addr, dek_blob_addr;

	debug("== Second AHAB container.\n");
	container_addr = addr + CONTAINER_HDR_ALIGNMENT;
	dek_blob_addr = get_blob_addr_from_container(container_addr);
	if (dek_blob_addr < 0) {
		printf("Failed to get DEK Blob address.\n");
		return -1;
	}
	offset[0] = dek_blob_addr - addr;
	debug("DEK offset = 0x%x\n", offset[0]);

	/* Jump to third container */
	debug("== Third AHAB container.\n");
	container_addr = get_next_container_addr(container_addr);
	if (container_addr < 0) {
		printf("Failed to get third container address.\n");
		return -1;
	}
	dek_blob_addr = get_blob_addr_from_container(container_addr);
	if (dek_blob_addr < 0) {
		printf("Failed to get DEK Blob address.\n");
		return -1;
	}
	offset[1] = dek_blob_addr - addr;
	debug("DEK offset = 0x%x\n", offset[1]);

	return 0;
}

int get_dek_blob_size(ulong addr, u32 *size)
{
	struct generate_key_blob_hdr *blob_hdr;

	blob_hdr = (struct generate_key_blob_hdr *)addr;
	if (blob_hdr->tag != AHAB_BLOB_HDR_TAG
	    || blob_hdr->version != AHAB_BLOB_HDR_VER) {
		debug("Error: wrong DEK blob header\n");
		return -1;
	}
	*size = blob_hdr->length_lsb + (blob_hdr->length_msb << 8);
	debug("DEK blob size is 0x%04x\n", *size);

	return 0;
}

/*
 * Get DEK blob from AHAB container in EMMC and copy to "addr"
 */
int get_dek_blob(ulong addr, u32 *size)
{
	struct mmc *mmc;
	u32 buf_size, offset;
	u8 *buf, part;
	int ret;
	ulong dek_blob_addr;
	u32 dek_blob_size;

	mmc = find_mmc_device(mmc_get_bootdevindex());
	if (!mmc) {
		printf("Failed to get mmc device.\n");
		return -1;
	}

	offset = CONTAINER_HDR_EMMC_OFFSET;
	part = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
	/* Fail if booting from user partition */
	if (part == 7) {
		printf("DEK blob from user partition not supported.\n");
		return -1;
	}
	ret = blk_select_hwpart(mmc_get_blk_desc(mmc)->bdev, part);
	if (ret) {
		printf("Failed to switch to boot partition %d.\n", part);
		return -1;
	}

	/*
	 * Read 4KB to be sure we get the DEK blob:
	 *
	 * CONTAINER_HDR_ALIGNMENT (1KB) + extra 3KB of header info
	 */
	buf_size = roundup(SZ_4K, mmc->read_bl_len);
	buf = malloc(buf_size);
	if (!buf)
		return -1;

	if (!blk_dread
	    (mmc_get_blk_desc(mmc), offset / mmc->read_bl_len,
	     buf_size / mmc->read_bl_len, buf)) {
		printf("Read container image from MMC/SD failed.\n");
		ret = -1;
		goto sanitize;
	}

	/* Recover the DEK blob and copy to 'addr' */
	dek_blob_addr =
	    get_blob_addr_from_container((ulong) buf + CONTAINER_HDR_ALIGNMENT);
	if (dek_blob_addr < 0) {
		printf("Failed to get DEK Blob address.\n");
		ret = -1;
		goto sanitize;
	}
	ret = get_dek_blob_size(dek_blob_addr, &dek_blob_size);
	if (ret) {
		debug("Failed to get DEK blob size.\n");
		goto sanitize;
	}
	memcpy((void *)addr, (void *)dek_blob_addr, dek_blob_size);

sanitize:
	memset(buf, 0, buf_size);
	free(buf);
	buf = NULL;

	return ret;
}

int get_os_container_img_offset(ulong addr)
{
	struct boot_img_t *img;
	struct container_hdr *phdr;

	phdr = (struct container_hdr *)addr;
	if (phdr->tag != AHAB_CNTR_HDR_TAG
	    || phdr->version != AHAB_CNTR_HDR_VER) {
		printf("Error: Wrong container header\n");
		return -1;
	}

	img = (struct boot_img_t *)(addr + sizeof(struct container_hdr));
	debug("OS container image offset: %u\n", img->offset);

	return img->offset;
}

int get_os_container_size(ulong addr)
{
	struct boot_img_t *img;
	struct container_hdr *phdr;
	u32 len;

	phdr = (struct container_hdr *)addr;
	if (phdr->tag != AHAB_CNTR_HDR_TAG
	    || phdr->version != AHAB_CNTR_HDR_VER) {
		printf("Error: Wrong container header\n");
		return -1;
	}

	img = (struct boot_img_t *)(addr + sizeof(struct container_hdr));
	len = img->offset + img->size;
	debug("OS container size: %u\n", len);

	return len;
}

bool is_container_encrypted(ulong addr, ulong *dek_addr)
{
	struct container_hdr *phdr;
	struct signature_block_hdr *sign_hdr;
	bool is_encrypted = false;

	phdr = (struct container_hdr *)(addr);
	if (phdr->tag != AHAB_CNTR_HDR_TAG
	    || phdr->version != AHAB_CNTR_HDR_VER)
		goto err_out;

	sign_hdr =
	    (struct signature_block_hdr *)((ulong)phdr + phdr->sig_blk_offset);
	if (sign_hdr->tag != AHAB_SIGN_HDR_TAG
	    || sign_hdr->version != AHAB_SIGN_HDR_VER)
		goto err_out;

	is_encrypted = (sign_hdr->blob_offset != 0);
	if (is_encrypted && dek_addr)
		*dek_addr = (ulong)sign_hdr + sign_hdr->blob_offset;

err_out:
	return is_encrypted;
}
