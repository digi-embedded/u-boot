/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#include <common.h>
#include <part.h>
#include "helper.h"

static int write_firmware(char *partname, disk_partition_t *info)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	char *filesize = getenv("filesize");
	unsigned long size;

	if (NULL == filesize) {
		debug("Cannot determine filesize\n");
		return -1;
	}
	size = simple_strtoul(filesize, NULL, 16) / CONFIG_SYS_STORAGE_BLKSZ;
	if (simple_strtoul(filesize, NULL, 16) % CONFIG_SYS_STORAGE_BLKSZ)
		size++;

	if (size > info->size) {
		printf("File size (%lu bytes) exceeds partition size (%lu bytes)!\n",
			size * CONFIG_SYS_STORAGE_BLKSZ,
			info->size * CONFIG_SYS_STORAGE_BLKSZ);
		return -1;
	}

	/* Prepare command to change to storage device */
	sprintf(cmd, "%s dev %d", CONFIG_SYS_STORAGE_MEDIA,
		CONFIG_SYS_STORAGE_DEV);

#ifdef CONFIG_SYS_BOOT_PART
	/* If U-Boot and special partition, append the hardware partition */
	if (!strcmp(partname, "uboot"))
		strcat(cmd, " $mmcbootpart");
#endif

	/* Change to storage device */
	if (run_command(cmd, 0)) {
		debug("Cannot change to storage device\n");
		return -1;
	}

	/* Prepare write command */
	sprintf(cmd, "%s write $loadaddr %lx %lx", CONFIG_SYS_STORAGE_MEDIA,
		info->start, size);

	return run_command(cmd, 0);
}

static int do_update(cmd_tbl_t* cmdtp, int flag, int argc, char * const argv[])
{
	int src = SRC_TFTP;	/* default to TFTP */
	char *devpartno = NULL;
	char *fs = NULL;
	disk_partition_t info;
	int ret;
	char filename[256] = "";
	char *forced_update;
	int abort = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Get data of partition to be updated */
	ret = get_target_partition(argv[1], &info);
	if (ret) {
		printf("Partition '%s' not found\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	/* Get source of update firmware file */
	if (argc > 2) {
		src = get_source(argv[2]);
		if (src == SRC_UNDEFINED)
			return CMD_RET_USAGE;
		if (src == SRC_USB || src == SRC_MMC || src == SRC_SATA) {
			/* Get device:partition and file system */
			if (argc > 3)
				devpartno = argv[3];
			if (argc > 4)
				fs = argv[4];
		}
	}

	/* Get firmware file name */
	ret = get_fw_filename(argc, argv, src, filename);
	if (ret) {
		/* Filename was not provided. Look for default one */
		ret = get_default_filename(argv[1], filename);
		if (ret) {
			printf("Error: need a filename\n");
			return CMD_RET_FAILURE;
		}
	}

	/* Load firmware file to RAM */
	ret = load_firmware_to_ram(src, filename, devpartno, fs,
				   "$loadaddr", NULL);
	if (ret < 0) {
		printf("Error loading firmware file to RAM\n");
		return CMD_RET_FAILURE;
	}

	forced_update = getenv("forced_update");
	if (!forced_update || strcmp(forced_update, "yes")) {
		/* Confirm programming */
		if (!strcmp((char *)info.name, "uboot") &&
		    !confirm_msg("Do you really want to program "
				 "the boot loader? <y/N> "))
			abort = 1;
	}

	if (!abort) {
		/* Write firmware file from RAM to storage */
		ret = write_firmware(argv[1], &info);
		if (ret) {
			printf("Error writing firmware\n");
			return CMD_RET_FAILURE;
		}
	}

	return 0;
}

U_BOOT_CMD(
	update,	6,	0,	do_update,
	"Digi modules update commands",
	"<partition>  [source] [extra-args...]\n"
	" Description: updates flash <partition> via <source>\n"
	" Arguments:\n"
	"   - partition:    a partition name or one of the reserved names: \n"
	"                   uboot\n"
	"   - [source]:     tftp (default)|nfs|usb|mmc|sata|ram\n"
	"   - [extra-args]: extra arguments depending on 'source'\n"
	"\n"
	"      source=tftp|nfs -> [filename]\n"
	"       - filename: file to transfer (required if using a partition name)\n"
	"\n"
	"      source=usb|mmc|sata -> [device:part] [filesystem] [filename]\n"
	"       - device:part: number of device and partition\n"
	"       - filesystem: fat|ext2|ext3\n"
	"       - filename: file to transfer\n"
	"\n"
	"      source=ram -> <image_address> <image_size>\n"
	"       - image_address: address of image in RAM\n"
	"       - image_size: size of image in RAM\n"
);
