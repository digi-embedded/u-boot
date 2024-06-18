/*
 *  Copyright (C) 2017 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <asm/cache.h>
#include <common.h>
#ifdef CONFIG_FSL_ESDHC_IMX
#include <fsl_esdhc_imx.h>
#endif
#include <env.h>
#include <mmc.h>
#include <malloc.h>
#include <otf_update.h>

#define ALIGN_SUP(x, a) (((x) + (a - 1)) & ~(a - 1))

extern int mmc_get_bootdevindex(void);

/*
 * Common function to get the block size of the storage media.
 */
size_t media_get_block_size(void)
{
	size_t block_size = 0;
	static struct blk_desc *mmc_dev;

	mmc_dev = blk_get_devnum_by_type(IF_TYPE_MMC, EMMC_BOOT_DEV);
	if (!mmc_dev)
		return block_size;

	block_size = mmc_dev->blksz;
	return block_size;
}

/*
 * Get the offset of a partition in the mmc
 * @in: Partition name
 * @in: Offset of the partition in mmc block units
 * return 0 if success
*/
int get_partition_offset(char *part_name, lbaint_t *offset)
{
	struct disk_partition info;
	char dev_index_str[2];
	int r;

	sprintf(dev_index_str, "%d", mmc_get_bootdevindex());
	r = get_partition_bynameorindex(CONFIG_SYS_STORAGE_MEDIA,
					dev_index_str,
					part_name,
					&info);
	if (r < 0)
		return r;
	*offset = info.start;
	return 0;
}

/*
 * Read a data block from the storage media.
 * This function only reads one mmc block
 * @in: Address in media (must be aligned to block size)
 * @in: Read buffer
 * @in: Partition index, only applies for MMC
 * The function returns:
 *	0 if success and data in readbuf.
 *	-1 on error
 */
int media_read_block(uintptr_t addr, unsigned char *readbuf, uint hwpart)
{
	size_t len;
	size_t nbytes;
	int ret = -1;
	static struct blk_desc *mmc_dev;
	uint orig_part;
	void *loadaddr = (void *) env_get_ulong("loadaddr", 16, CONFIG_LOADADDR);

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	mmc_dev = blk_get_devnum_by_type(IF_TYPE_MMC, EMMC_BOOT_DEV);
	if (!mmc_dev)
		return ret;

	orig_part = mmc_dev->hwpart;
	if (blk_dselect_hwpart(mmc_dev, hwpart))
		return ret;

	nbytes = blk_dread(mmc_dev, addr, 1, loadaddr);
	if (nbytes == 1) {
		memcpy(readbuf, loadaddr, len);
		ret = 0;
	}
	blk_dselect_hwpart(mmc_dev, orig_part);
	return ret;
}

/*
 * Write a data block to the storage media.
 * This function only writes the minimum required amount of data:
 *	for mmc: one block (probably 512 bytes)
 * @in: Address in media (must be aligned to block size)
 * @in: Write buffer
 * @in: Partition index, only applies for MMC
 * The function returns:
 *	0 if success
 *	-1 on error
 */
int media_write_block(uintptr_t addr, unsigned char *writebuf, uint hwpart)
{
	size_t len;
	int ret = -1;
	static struct blk_desc *mmc_dev;
	unsigned long written;
	uint orig_part;

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	mmc_dev = blk_get_devnum_by_type(IF_TYPE_MMC, EMMC_BOOT_DEV);
	if (!mmc_dev)
		return ret;

	orig_part = mmc_dev->hwpart;
	if (blk_dselect_hwpart(mmc_dev, hwpart))
		return ret;

	written = blk_dwrite(mmc_dev, addr, 1, writebuf);
	if (written == 1)
		ret = 0;

	blk_dselect_hwpart(mmc_dev, orig_part);
	return ret;
}

uint get_env_hwpart(void)
{
	struct mmc *mmc;

	mmc = find_mmc_device(mmc_get_bootdevindex());
	return mmc_get_env_part(mmc);
}

// #ifdef CONFIG_FSL_ESDHC_IMX
extern int mmc_get_bootdevindex(void);

static int write_chunk(struct mmc *mmc, struct blk_desc *mmc_dev,
		       otf_data_t *otfd, lbaint_t dstblk,
		       unsigned int chunklen)
{
	int sectors;
	unsigned long written, read;
	void *verifyaddr;

	printf("\nWriting chunk...");
	/* Check WP */
	if (mmc_getwp(mmc) == 1) {
		printf("[Error]: card is write protected!\n");
		return -1;
	}

	/* We can only write whole sectors (multiples of mmc_dev->blksz bytes)
	 * so we need to check if the chunk is a whole multiple or else add 1
	 */
	sectors = chunklen / mmc_dev->blksz;
	if (chunklen % mmc_dev->blksz)
		sectors++;

	/* Check if chunk fits */
	if (sectors + dstblk > otfd->part->start + otfd->part->size) {
		printf("[Error]: length of data exceeds partition size\n");
		return -1;
	}

	/* Write chunklen bytes of chunk to media */
	debug("writing chunk of 0x%x bytes (0x%x sectors) "
		"from 0x%p to block " LBAFU "\n",
		chunklen, sectors, otfd->loadaddr, dstblk);
	written = blk_dwrite(mmc_dev, dstblk, sectors, otfd->loadaddr);
	if (written != sectors) {
		printf("[Error]: written sectors != sectors to write\n");
		return -1;
	}
	printf("[OK]\n");

	/* Verify written chunk if $loadaddr + chunk size does not overlap
	 * $verifyaddr (where the read-back copy will be placed)
	 */
	verifyaddr = (void *) env_get_ulong("verifyaddr", 16, 0);
	if (otfd->loadaddr + sectors * mmc_dev->blksz < verifyaddr) {
		/* Read back data... */
		printf("Reading back chunk...");
		read = blk_dread(mmc_dev, dstblk, sectors, verifyaddr);
		if (read != sectors) {
			printf("[Error]: read sectors != sectors to read\n");
			return -1;
		}
		printf("[OK]\n");
		/* ...then compare */
		printf("Verifying chunk...");
		if (memcmp(otfd->loadaddr, verifyaddr,
			      sectors * mmc_dev->blksz)) {
			printf("[Error]\n");
			return -1;
		} else {
			printf("[OK]\n");
			return 0;
		}
	} else {
		printf("[Warning]: Cannot verify chunk. "
			"It overlaps $verifyaddr!\n");
		return 0;
	}
}

#ifdef CONFIG_FASTBOOT_FLASH
static lbaint_t sparse_write(struct sparse_storage *info, lbaint_t blk,
			     lbaint_t blkcnt, const void *buffer)
{
       otf_sparse_data_t *data = info->priv;

       return blk_dwrite(data->mmc_dev, blk, blkcnt, buffer);
}

static lbaint_t sparse_reserve(struct sparse_storage *info, lbaint_t blk,
			       lbaint_t blkcnt)
{
       return blkcnt;
}

static int sparse_write_bytes(otf_sparse_data_t *sparse_data, lbaint_t *dstblk,
			      void *data, uint32_t data_length)
{
	const unsigned long blksz = sparse_data->mmc_dev->blksz;
	lbaint_t blks_to_write = (data_length / blksz) + (data_length % blksz > 0);
	lbaint_t blks_written = sparse_write(&sparse_data->storage_info, *dstblk,
					     blks_to_write, data);

	if (blks_written != blks_to_write) {
		printf(" [ERROR]\nWrite failed! at block # " LBAFU " (requested: " LBAFU ", written: " LBAFU ")\n\n",
			   *dstblk, blks_to_write, blks_written);
		return -1;
	}

	*dstblk += blks_written;

	return 0;
}

static uint32_t write_sparse_chunks(otf_data_t *otfd, lbaint_t *dstblk,
				    unsigned int chunk_len)
{
	const unsigned long blksz = otfd->sparse_data.mmc_dev->blksz;
	otf_sparse_data_t *sp_data = &otfd->sparse_data;
	int offset_within_chunk = sp_data->align_offset;

	/* On the first block, get and save the sparse image header */
	if (!(otfd->flags & OTF_FLAG_SPARSE_HDR)) {
		const sparse_header_t *hdr = (sparse_header_t *) otfd->loadaddr;

		memcpy(&sp_data->hdr, hdr, sizeof(sp_data->hdr));
		otfd->flags |= OTF_FLAG_SPARSE_HDR;
		offset_within_chunk += sp_data->hdr.file_hdr_sz;

		debug("\n\n=== Sparse Image Header === @ 0x%p\n", hdr);
		debug("magic: 0x%x\n", hdr->magic);
		debug("major_version: 0x%x\n", hdr->major_version);
		debug("minor_version: 0x%x\n", hdr->minor_version);
		debug("file_hdr_sz: %d\n", hdr->file_hdr_sz);
		debug("chunk_hdr_sz: %d\n", hdr->chunk_hdr_sz);
		debug("blk_sz: %d\n", hdr->blk_sz);
		debug("total_blks: %d\n", hdr->total_blks);
		debug("total_chunks: %d\n", hdr->total_chunks);
	}

	printf("\nWriting sparse chunks (%d/%d)... ", sp_data->current_chunk,
	       sp_data->hdr.total_chunks);

	/*
	 * If a raw chunk is in the process of being flashed, deal with that first.
	 */
	if (otfd->flags & OTF_FLAG_RAW_ONGOING) {
		void *data = otfd->loadaddr + offset_within_chunk;
		uint32_t bytes_to_write = sp_data->ongoing_bytes -
					  sp_data->ongoing_bytes_written;
		uint32_t available = chunk_len - offset_within_chunk;

		/* If there is still more data to come, round to the block size */
		if (bytes_to_write > available)
			bytes_to_write = available - (available % blksz);

		if (sparse_write_bytes(sp_data, dstblk, data, bytes_to_write))
			return -1;

		sp_data->ongoing_bytes_written += bytes_to_write;
		offset_within_chunk += bytes_to_write;

		/* Check if we finished with this raw sparse chunk */
		if (sp_data->ongoing_bytes == sp_data->ongoing_bytes_written) {
			/* We account here for all the bytes and blocks of this chunk */
			sp_data->bytes_written += sp_data->ongoing_bytes;
			sp_data->blks_written += sp_data->ongoing_blks;
			otfd->flags &= ~OTF_FLAG_RAW_ONGOING;

			debug("Chunk %d (RAW) completed. (0x%x bytes)\n",
			      sp_data->current_chunk, sp_data->ongoing_bytes_written);
			debug("\n\n");

			sp_data->current_chunk++;
		} else {
			debug("Chunk %d (RAW) partially flashed (0x%x/0x%x)\n",
			      sp_data->current_chunk, sp_data->ongoing_bytes_written,
			      sp_data->ongoing_bytes);
		}
	}

	/*
	 * If we are not in the middle of flashing a raw chunk, deal with all the sparse
	 * chunks that fitted in the current OTF chunk.
	 */
	while (!(otfd->flags & OTF_FLAG_RAW_ONGOING) &&
	       offset_within_chunk + sizeof(chunk_header_t) <= chunk_len &&
	       sp_data->current_chunk < sp_data->hdr.total_chunks) {
		void *data = otfd->loadaddr + offset_within_chunk;
		const chunk_header_t *chunk_hdr = (chunk_header_t *) data;

		debug("=== Chunk Header (%d/%d) === @ 0x%p\n",
		      sp_data->current_chunk, sp_data->hdr.total_chunks, chunk_hdr);
		debug("chunk_type: 0x%x\n", chunk_hdr->chunk_type);
		debug("chunk_data_sz: 0x%x\n", chunk_hdr->chunk_sz);
		debug("total_size: 0x%x\n", chunk_hdr->total_sz);
		debug("range in chunk: [0x%x, 0x%x)\n",
		      offset_within_chunk, offset_within_chunk + chunk_hdr->total_sz);
		debug("chunk len: 0x%x\n", chunk_len);
		debug("\n");

		if (offset_within_chunk + chunk_hdr->total_sz <= chunk_len) {
			/*
			 * The current sparse chunk fits in the current OTF chunk.
			 * Process it and move on.
			 */
			if (write_sparse_chunk(&sp_data->storage_info, &sp_data->hdr,
					       &data, dstblk, &sp_data->blks_written,
					       &sp_data->bytes_written))
				return -1;

			debug("Chunk %d completed\n", sp_data->current_chunk);
			debug("\n\n");

			offset_within_chunk += chunk_hdr->total_sz;
			sp_data->current_chunk++;
		} else {
			/*
			 * The current sparse chunk did not fit in the OTF chunk.
			 *
			 * For sparse chunks other than CHUNK_TYPE_RAW, we can just break out
			 * of the loop. The remaing data in the current OTF chunk will be
			 * moved to the start of the next OTF chunk and the current
			 * incomplete sparse chunk will be dealt with in the next iteration.
			 * It is safe to do this because these sparse chunks are always
			 * smaller than BLK_SIZE.
			 *
			 * For CHUNK_TYPE_RAW sparse chunks we cannot use that approach
			 * because they could be bigger than BLK_SIZE. Thus, write as
			 * much as we can and flag this condition. On the next OTF chunk, we
			 * will keep processing the current incomplete raw sparse chunk.
			 * (Note that we need at least the complete chunk header, otherwise
			 * we break until the next OTF chunk)
			 */
			if (chunk_hdr->chunk_type != CHUNK_TYPE_RAW) {
				/* Deal with this in the next OTF chunk */
				debug("Chunk %d did not fit\n", sp_data->current_chunk);
				break;
			} else {
				uint32_t bytes_left;
				uint32_t bytes_to_write;

				debug("Chunk %d (RAW) did not fit\n", sp_data->current_chunk);

				/* Set RAW_ONGOING state */
				otfd->flags |= OTF_FLAG_RAW_ONGOING;
				offset_within_chunk += sizeof(chunk_header_t);
				sp_data->ongoing_bytes_written = sizeof(chunk_header_t);
				sp_data->ongoing_bytes = chunk_hdr->total_sz;
				sp_data->ongoing_blks = chunk_hdr->chunk_sz;

				/* Partially write this CHUNK_TYPE_RAW, as much as we can. */
				data += sizeof(chunk_header_t); /* skip header */
				bytes_left = chunk_len - offset_within_chunk;
				bytes_to_write = bytes_left - (bytes_left % blksz);

				if (sparse_write_bytes(sp_data, dstblk, data, bytes_to_write))
					return -1;

				sp_data->ongoing_bytes_written += bytes_to_write;
				offset_within_chunk += bytes_to_write;

				debug("Chunk %d (RAW) partially flashed (0x%x/0x%x)\n",
				      sp_data->current_chunk, sp_data->ongoing_bytes_written,
				      sp_data->ongoing_bytes);
			}
		}
	}

	printf("[OK]\n");
	debug("OTF chunk completed, 0x%x bytes used (0x%x available)\n",
	      offset_within_chunk, chunk_len);

	return offset_within_chunk;
}
#endif

/* writes a chunk of data from RAM to main storage media (eMMC) */
int update_chunk(otf_data_t *otfd)
{
	static unsigned int chunk_len = 0;
	static lbaint_t dstblk = 0;
	static struct blk_desc *mmc_dev = NULL;
	static int mmc_dev_index = -1;
	static struct mmc *mmc = NULL;

	if (mmc_dev_index == -1) {
		mmc_dev_index = env_get_ulong("mmcdev", 16,
					     mmc_get_bootdevindex());
		mmc = find_mmc_device(mmc_dev_index);
		if (NULL == mmc) {
			printf("ERROR: failed to get MMC device\n");
			return -1;
		}
		mmc_dev = mmc_get_blk_desc(mmc);
		if (NULL == mmc_dev) {
			printf("ERROR: failed to get block descriptor for MMC device\n");
			return -1;
		}
	}

	/*
	 * There are two variants:
	 *  - otfd.buf == NULL
	 *  	In this case, the data is already waiting on the correct
	 *  	address in RAM, waiting to be written to the media.
	 *  - otfd.buf != NULL
	 *  	In this case, the data is on the buffer and must still be
	 *  	copied to an address in RAM, before it is written to media.
	 */
	if (otfd->buf && otfd->len) {
		/*
		 * If data is in the otfd->buf buffer, copy it to the loadaddr
		 * in RAM until we have a chunk that is at least as large as
		 * CONFIG_OTF_CHUNK, to write it to media.
		 */
		memcpy(otfd->loadaddr + otfd->offset, otfd->buf, otfd->len);
	}

	/* Initialize dstblk and local variables */
	if (otfd->flags & OTF_FLAG_INIT) {
		chunk_len = 0;
		dstblk = otfd->part->start;
		otfd->flags &= ~OTF_FLAG_INIT;

#ifdef CONFIG_FASTBOOT_FLASH
		if (is_sparse_image(otfd->loadaddr)) {
			const sparse_header_t *hdr = (sparse_header_t *) otfd->loadaddr;
			otfd->flags |= OTF_FLAG_SPARSE;

			/* Ensure that the sparse image fits in the partition */
			if (hdr->blk_sz * hdr->total_blks >
			    mmc_dev->blksz * otfd->part->size) {
				printf("\n\n[ERROR] Sparse image does not fit in the partition.\n");
				return -1;
			}

			memset(&otfd->sparse_data, 0, sizeof(otfd->sparse_data));
			otfd->sparse_data.storage_info = (struct sparse_storage) {
				.blksz = mmc_dev->blksz,
				.start = otfd->part->start,
				.size = otfd->part->size,
				.write = sparse_write,
				.reserve = sparse_reserve,
				.priv = &otfd->sparse_data,
			};
			otfd->sparse_data.mmc_dev = mmc_dev;
		}
#endif
	}
	chunk_len += otfd->len;

	/*
	 * If the chunk is completed or if it is the last chunk (OTF_FLAG_FLUSH
	 * set) write it to storage.
	 */
	if (chunk_len >= CONFIG_OTF_CHUNK || (otfd->flags & OTF_FLAG_FLUSH)) {
		unsigned int remaining;

#ifdef CONFIG_FASTBOOT_FLASH
		if (otfd->flags & OTF_FLAG_SPARSE) {
			int align_offset = 0;
			uint32_t bytes_used = write_sparse_chunks(otfd, &dstblk, chunk_len);

			if (bytes_used == (uint32_t) -1)
				return -1;

			remaining = chunk_len - bytes_used;

			/*
			 * Because of a limitation of the 'load' command, we
			 * need to offset the remaining data so that its end is
			 * aligned to ARCH_DMA_MINALIGN. Otherwise we get
			 * warnings when reading the next data from a file.
			 */
			align_offset = ALIGN_SUP(remaining, ARCH_DMA_MINALIGN) - remaining;
			otfd->sparse_data.align_offset = align_offset;
			remaining += align_offset;

			chunk_len -= remaining;
		} else
#endif
		{
			if (otfd->flags & OTF_FLAG_FLUSH) {
				/* Write all pending data (this is the last chunk) */
				remaining = 0;
			} else {
				/* We have CONFIG_OTF_CHUNK (or more) bytes in RAM.
				 * Let's proceed to write as many as multiples of blksz
				 * as possible.
				 */
				remaining = chunk_len % mmc_dev->blksz;
				chunk_len -= remaining;
				/* chunk_len is now multiple of blksz */
			}

			if (write_chunk(mmc, mmc_dev, otfd, dstblk, chunk_len))
				return -1;

			/* increment destiny block */
			dstblk += (chunk_len / mmc_dev->blksz);
		}

		/* copy excess of bytes from previous chunk to offset 0 */
		if (remaining) {
			memcpy(otfd->loadaddr, otfd->loadaddr + chunk_len,
			       remaining);
			debug("Copying excess of %d bytes to offset 0\n",
			      remaining);
		}
		/* reset chunk_len to excess of bytes from previous chunk
		 * (or zero, if that's the case) */
		chunk_len = remaining;
	}

	/*
	 * This flag signals the last block, so we have finished.
	 * Reset all static variables.
	 */
	if (otfd->flags & OTF_FLAG_FLUSH) {
		chunk_len = 0;
		dstblk = 0;
		mmc_dev = NULL;
		mmc_dev_index = -1;
		mmc = NULL;
#ifdef CONFIG_FASTBOOT_FLASH
		/* For sparse images check that everything was flashed */
		if (otfd->flags & OTF_FLAG_SPARSE) {
			const otf_sparse_data_t *sparse_data = &otfd->sparse_data;

			if (sparse_data->hdr.total_chunks != sparse_data->current_chunk) {
				printf("Sparse image flashing failed. 0x%x chunks expected, only 0x%x written\n",
				       sparse_data->hdr.total_chunks,
				       sparse_data->current_chunk);
				return -1;
			}

			if (sparse_data->hdr.total_blks != sparse_data->blks_written) {
				printf("Sparse image flashing failed. 0x%x blocks expected, only 0x%x written\n",
				       sparse_data->hdr.total_blks,
				       sparse_data->blks_written);
				return -1;
			}
		}
#endif
		return 0;
	}

	/*
	 * Set otfd offset pointer to offset in RAM where new bytes would
	 * be written. Data in the interval
	 * [otfd->loadaddr, otfd->loadaddr + otfd->offset) shall not be
	 * replaced with new data. The rest of the buffer may be reused
	 * by the caller
	 */
	otfd->offset = chunk_len;

	return 0;
}
// #endif /* CONFIG_FSL_ESDHC_IMX */

#if defined(CONFIG_CMD_UPDATE_MMC) && defined(EMMC_BOOT_PART_OFFSET)
extern void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
					    struct disk_partition *info);
#endif
ulong bootloader_mmc_offset(void)
{
#if defined(CONFIG_CMD_UPDATE_MMC) && defined(EMMC_BOOT_PART_OFFSET)
	struct disk_partition info;
	int mmc_dev_index = env_get_ulong("mmcdev", 16, mmc_get_bootdevindex());
	struct blk_desc *mmc_dev = blk_get_devnum_by_type(IF_TYPE_MMC,
							  mmc_dev_index);
	if (NULL == mmc_dev) {
		debug("Cannot determine sys storage device\n");
		return EMMC_BOOT_PART_OFFSET;
	}

	calculate_uboot_update_settings(mmc_dev, &info);

	return info.start * mmc_dev->blksz;
#else
	return 0;
#endif
}
