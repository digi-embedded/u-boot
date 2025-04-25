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
 * Get the offset of a partition in the nand by parsing the mtdparts
 * @in: Partition name
 * return 0 if found or -1 if error
*/
int get_partition_offset(char *part_name, lbaint_t *offset)
{
	char *parts, *parttable;
	char *aux = NULL;
	char *name_partition = NULL;
	int ret = -1;

	parttable = strdup(env_get("mtdparts"));
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

/* Dummy function for compatibility with MMC */
uint get_env_hwpart(void)
{
	return 0;
}
