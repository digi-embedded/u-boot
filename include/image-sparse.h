/*
 * Copyright 2014 Broadcom Corporation.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __IMAGE_SPARSE_H
#define __IMAGE_SPARSE_H

#include <part.h>
#include <sparse_format.h>

#define ROUNDUP(x, y)	(((x) + ((y) - 1)) & ~((y) - 1))

struct sparse_storage {
	lbaint_t	blksz;
	lbaint_t	start;
	lbaint_t	size;
	void		*priv;

	lbaint_t	(*write)(struct sparse_storage *info,
				 lbaint_t blk,
				 lbaint_t blkcnt,
				 const void *buffer);

	lbaint_t	(*reserve)(struct sparse_storage *info,
				 lbaint_t blk,
				 lbaint_t blkcnt);
};

static inline int is_sparse_image(void *buf)
{
	sparse_header_t *s_header = (sparse_header_t *)buf;

	if ((le32_to_cpu(s_header->magic) == SPARSE_HEADER_MAGIC) &&
	    (le16_to_cpu(s_header->major_version) == 1))
		return 1;

	return 0;
}

int write_sparse_chunk(struct sparse_storage *info,
		       const sparse_header_t *sparse_header, void **data_ptr,
		       lbaint_t *blk, uint32_t *total_blocks,
		       uint32_t *bytes_written);

int write_sparse_image(struct sparse_storage *info, const char *part_name,
			void *data, unsigned sz);

#endif /* __IMAGE_SPARSE_H */