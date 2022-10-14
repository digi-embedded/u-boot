/*
 *  Copyright (C) 2017 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <common.h>
#include <nand.h>
#include "helper.h"
#include <env.h>

/*
 * Get the block size of the storage media.
 */
size_t media_get_block_size(void)
{
	return get_nand_dev_by_index(0)->erasesize;
}

/*
 * Read data from the storage media.
 * This function only reads one nand erase block
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

	len = media_get_block_size();
	if (len <= 0)
		return -1;

	return nand_read_skip_bad(get_nand_dev_by_index(0),
				  addr,
				  &len,
				  NULL,
				  get_nand_dev_by_index(0)->size,
				  readbuf);
}

/*
 * Write data to the storage media.
 * This function only writes the minimum required amount of data:
 *	for nand: one erase block size
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

	len = media_get_block_size();
	if (len <= 0)
		return -1;

	return nand_write_skip_bad(get_nand_dev_by_index(0),
				   addr,
				   &len,
				   NULL,
				   get_nand_dev_by_index(0)->size,
				   writebuf,
				   WITH_WR_VERIFY);
}
