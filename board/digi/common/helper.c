/*
 *  common/helper.c
 *
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#include <common.h>
#include "helper.h"

int confirm_msg(char *msg)
{
	printf(msg);
	if (getc() == 'y') {
		int c;

		putc('y');
		c = getc();
		putc('\n');
		if (c == '\r')
			return 1;
	}

	puts("Operation aborted by user\n");
	return 0;
}

int get_image_source(char *source)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(image_source_str); i++) {
		if (!strncmp(image_source_str[i], source, strlen(source)))
			return i;
	}

	return IS_UNDEFINED;
}

int get_target_partition(char *partname, disk_partition_t *info)
{
	if (!strcmp(partname, "uboot")) {
		/* Simulate partition data for U-Boot */
		info->start = CONFIG_SYS_BOOT_PART_OFFSET /
			      CONFIG_SYS_STORAGE_BLKSZ;
		info->size = CONFIG_SYS_BOOT_PART_SIZE /
			     CONFIG_SYS_STORAGE_BLKSZ;
		strcpy((char *)info->name, partname);
	} else {
		/* Not a reserved name. Must be a partition name */
		/* Look up the device */
		if (get_partition_byname(CONFIG_SYS_STORAGE_MEDIA,
					 __stringify(CONFIG_SYS_STORAGE_DEV),
					 partname, info) < 0)
			return -1;
	}
	return 0;
}

int get_fw_filename(int argc, char * const argv[], image_source_e src,
			   char *filename)
{
	switch (src) {
	case IS_TFTP:
	case IS_NFS:
		if (argc > 3) {
			strcpy(filename, argv[3]);
			return 0;
		}
		break;
	case IS_MMC:
	case IS_USB:
	case IS_SATA:
		if (argc > 5) {
			strcpy(filename, argv[5]);
			return 0;
		}
		break;
	case IS_RAM:
		return 0;	/* No file is needed */
	default:
		return -1;
	}

	return -1;
}

int get_default_filename(char *partname, char *filename)
{
	if (!strcmp(partname, "uboot")) {
		strcpy(filename, "$uboot_file");
		return 0;
	}

	return -1;
}
