/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __OTF_UPDATE_H
#define __OTF_UPDATE_H

#ifdef CONFIG_FASTBOOT_FLASH
#include <image-sparse.h>
#endif

/* OTF flags */
#define OTF_FLAG_FLUSH 		(1 << 0) /* write last chunk */
#define OTF_FLAG_INIT 		(1 << 1) /* write first chunk */
#define OTF_FLAG_SPARSE		(1 << 2) /* target is a sparse image */
#define OTF_FLAG_SPARSE_HDR 	(1 << 3) /* sparse header has been copied */
#define OTF_FLAG_RAW_ONGOING 	(1 << 4) /* raw sparse chunk being flashed */

#ifdef CONFIG_FASTBOOT_FLASH
typedef struct otf_sparse_data {
	sparse_header_t hdr;			/* sparse image header */
	struct sparse_storage storage_info;	/* for write_sparse_chunk() */
	int current_chunk;			/* sparse chunk index */
	struct blk_desc *mmc_dev;		/* blk device to write to */
	int align_offset;			/* to DMA-align pending data */

	/* status info for the complete sparse image */
	uint32_t blks_written;			/* blocks written so far */
	uint64_t bytes_written;			/* bytes written so far */

	/* status info for RAW block currently being flashed (if any) */
	uint32_t ongoing_bytes_written;		/* bytes written so far */
	uint32_t ongoing_bytes;			/* total bytes */
	lbaint_t ongoing_blks;			/* total blocks */
} otf_sparse_data_t;
#endif

typedef struct otf_data {
	void *loadaddr;		/* address in RAM to load data to */
	unsigned int offset;	/* offset in media to write data to */
	unsigned char *buf;	/* buffer with data to write */
	unsigned int len;	/* length of chunk to write */
	struct disk_partition *part;	/* partition data */
	unsigned int flags;	/* on-the-fly flags */
#ifdef CONFIG_FASTBOOT_FLASH
	otf_sparse_data_t sparse_data; /* info for sparse images */
#endif
} otf_data_t;

#endif  /* __OTF_UPDATE_H */
