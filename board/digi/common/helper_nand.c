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

/*
 * Get the block size of the storage media.
 */
size_t media_get_block_size(void)
{
	return nand_info[0].erasesize;
}

/*
 * Get the offset of a partition in the nand by parsing the mtdparts
 * @in: Partition name
 * return 0 if found or -1 if error
*/
int get_partition_offset(char *part_name, u32 *offset)
{
	char *parts, *parttable;
	char *aux = NULL;
	char *name_partition = NULL;
	int ret = -1;

	parttable = strdup(getenv("mtdparts"));
	if (!parttable)
		goto end;
	aux = parttable;
	/*
	 * parttable = "mtdparts=mtdparts=gpmi-nand:3m(bootloader),1m(environment),
	 *		1m(safe),12m(linux),14m(recovery),122m(rootfs),-(update)";
	 */
	parts = strsep(&parttable, ":");
	if (!parts)
		goto end;
	/*
	 *  3m(bootloader),1m(environment),1m(safe),12m(linux),
	 *   14m(recovery),122m(rootfs),-(update)";
	 */
	parts = strsep(&parttable, "(");
	*offset = 0;
	while (parts) {
		name_partition = strsep(&parttable, ")");
		if (!name_partition)
			break;
		if (!strcmp(name_partition, part_name)) {
			ret = 0;
			break;
		}
		*offset += memsize_parse(parts, (const char **)&parts);
		if (!strsep(&parttable, ","))
			break;
		parts = strsep(&parttable, "(");
	}
end:
	free(aux);
	return ret;
}

/*
 * Get the TF Key offset in the U-Boot environment
 */
unsigned int get_filesystem_key_offset(void)
{
	u32 offset;

	if (!get_partition_offset("environment", &offset))
		return offset + (2*media_get_block_size());
	return 0;
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
int media_read_block(u32 addr, unsigned char *readbuf, uint hwpart)
{
	size_t len;
	size_t nbytes;
	int ret = -1;
	int r;

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	r = nand_read_skip_bad(&nand_info[0],
				addr,
				&len,
				&nbytes,
				nand_info[0].size,
				readbuf);
	if (!r && nbytes == len)
		ret = 0;
	return ret;
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
int media_write_block(u32 addr, unsigned char *writebuf, uint hwpart)
{
	size_t len, nbytes;
	int ret = -1;
	int r;

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	r = nand_write_skip_bad(&nand_info[0],
				addr,
				&len,
				&nbytes,
				nand_info[0].size,
				writebuf,
				WITH_WR_VERIFY);
	if (!r && nbytes == len)
		ret = 0;
	return ret;
}

/*
 * Erase data in the storage media.
 * This function only erases the minimum required amount of data:
 *	for nand: one erase block size
 * @in: Address in media (must be aligned to block size)
 * @in: Partition index, only applies for MMC
 */
void media_erase_fskey(u32 addr, uint hwpart)
{
	nand_erase(&nand_info[0], addr, nand_info[0].erasesize);
}

/* Dummy function for compatibility with MMC */
uint get_env_hwpart(void)
{
	return 0;
}
