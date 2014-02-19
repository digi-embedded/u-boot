/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#include <common.h>
#include "helper.h"

enum {
	FWLOAD_NO,
	FWLOAD_YES,
	FWLOAD_TRY,
};

static const char *src_strings[] = {
	[SRC_TFTP] =	"tftp",
	[SRC_NFS] =	"nfs",
	[SRC_FLASH] =	"flash",
	[SRC_USB] =	"usb",
	[SRC_MMC] =	"mmc",
	[SRC_RAM] =	"ram",
	[SRC_SATA] =	"sata",
};

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

int get_source(char *src)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(src_strings); i++) {
		if (!strncmp(src_strings[i], src, strlen(src)))
			return i;
	}

	return SRC_UNDEFINED;
}

const char *get_source_string(int src)
{
	if (SRC_UNDEFINED != src && src < ARRAY_SIZE(src_strings))
		return src_strings[src];

	return "";
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

int get_fw_filename(int argc, char * const argv[], int src, char *filename)
{
	switch (src) {
	case SRC_TFTP:
	case SRC_NFS:
		if (argc > 3) {
			strcpy(filename, argv[3]);
			return 0;
		}
		break;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
		if (argc > 5) {
			strcpy(filename, argv[5]);
			return 0;
		}
		break;
	case SRC_RAM:
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
	} else if (!strcmp(partname, "linux") ||
		   !strcmp(partname, "android")) {
		strcpy(filename, "$uimage");
		return 0;
	}

	return -1;
}

/* A variable determines if the file must be loaded.
 * The function returns:
 * 	 1 if the file was loaded successfully
 * 	 0 if the file was not loaded, but isn't required
 * 	-1 on error
 */
int load_firmware_to_ram(int src, char *filename, char *devpartno,
			 char *fs, char *loadaddr, char *varload)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	char def_devpartno[] = "0:1";
	char def_fs[] = "fat";
	int ret;
	int fwload = FWLOAD_YES;

	/* Variable 'varload' determines if the file must be loaded:
	 * - yes|NULL: the file must be loaded. Return error otherwise.
	 * - try: the file may be loaded. Return ok even if load fails.
	 * - no: skip the load.
	 */
	if (NULL != varload) {
		if (!strcmp(varload, "no"))
			return 0;	/* skip load and return ok */
		else if (!strcmp(varload, "try"))
			fwload = FWLOAD_TRY;
	}

	/* Use default values if not provided */
	if (NULL == devpartno)
		devpartno = def_devpartno;
	if (NULL == fs)
		fs = def_fs;

	switch (src) {
	case SRC_TFTP:
		sprintf(cmd, "tftpboot %s %s", loadaddr, filename);
		break;
	case SRC_NFS:
		sprintf(cmd, "nfs %s $nfsroot/%s", loadaddr, filename);
		break;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
		sprintf(cmd, "%sload %s %s %s %s", fs,
			src_strings[src], devpartno, loadaddr, filename);
		break;
	case SRC_RAM:
		ret = 0;	/* file is already supposed to be in RAM */
		goto _ret;
	default:
		return -1;
	}

	ret = run_command(cmd, 0);
_ret:
	if (FWLOAD_TRY == fwload)
		return !ret;	/* 1 if file was loaded, 0 if not */

	if (ret)
		return -1;	/* error */

	return 1;	/* ok, file was loaded */
}
