/*
 *  Copyright (C) 2016 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <asm/imx-common/boot_mode.h>
#include <common.h>
#include <jffs2/load_kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <nand.h>
#include <otf_update.h>

#include "helper.h"

DECLARE_GLOBAL_DATA_PTR;

enum {
	ERR_WRITE = 1,
	ERR_READ,
	ERR_VERIFY,
};

static int write_firmware(unsigned long loadaddr, unsigned long filesize,
			  struct part_info *part)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	unsigned long verifyaddr, u, m;

	if (filesize > part->size) {
		printf("File size (%lu bytes) exceeds partition size (%lu bytes)!\n",
			filesize, (unsigned long)part->size);
		return -1;
	}

	/* Write firmware command */
	sprintf(cmd, "nand write %lx %s %lx", loadaddr, part->name, filesize);
	if (run_command(cmd, 0))
		return ERR_WRITE;

	/*
	 * If there is enough RAM to hold two copies of the firmware,
	 * verify written firmware.
	 * +--------|---------------------|------------------|--------------+
	 * |        L                     V                  | U-Boot+Stack |
	 * +--------|---------------------|------------------|--------------+
	 * P                                                 U              M
	 *
	 *  P = PHYS_SDRAM (base address of SDRAM)
	 *  L = $loadaddr
	 *  V = $verifyaddr
	 *  M = last address of SDRAM (CONFIG_DDR_MB (size of SDRAM) + P)
	 *  U = SDRAM address where U-Boot is located (plus margin)
	 */
	verifyaddr = getenv_ulong("verifyaddr", 16, 0);
	m = PHYS_SDRAM + gd->ram_size;
	u = m - CONFIG_UBOOT_RESERVED;

	/*
	 * ($loadaddr + firmware size) must not exceed $verifyaddr
	 * ($verifyaddr + firmware size) must not exceed U.
	 */
	if ((loadaddr + filesize) < verifyaddr &&
	    (verifyaddr + filesize) < u) {
		unsigned long filesize_padded;
		int i;

		/* Read back data... */
		printf("Reading back firmware...\n");
		sprintf(cmd, "nand read %lx %s %lx", verifyaddr, part->name,
			filesize);
		if (run_command(cmd, 0))
			return ERR_READ;
		/*
		 * ...then compare by 32-bit words (faster than by bytes)
		 * padding with zeros any bytes at the end to make the size
		 * be a multiple of 4.
		 *
		 * Reference: http://stackoverflow.com/a/2022252
		 */
		printf("Verifying firmware...\n");
		filesize_padded = (filesize + (4 - 1)) & ~(4 - 1);

		for (i = filesize; i < filesize_padded; i++) {
			*((char *)loadaddr + i) = 0;
			*((char *)verifyaddr + i) = 0;
		}
		sprintf(cmd, "cmp.l %lx %lx %lx", loadaddr, verifyaddr,
			(filesize_padded / 4));
		if (run_command(cmd, 0))
			return ERR_VERIFY;
		printf("Update was successful\n");
	} else {
		printf("Firmware updated but not verified "
		       "(not enough available RAM to verify)\n");
	}

	return 0;
}

static int do_update(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int src = SRC_TFTP;	/* default to TFTP */
	char *devpartno = NULL;
	char *fs = NULL;
	int ret;
	char filename[256] = "";
	int otf = 0;
	unsigned long loadaddr;
	unsigned long verifyaddr;
	unsigned long filesize = 0;
	struct mtd_device *dev;
	struct part_info *part;
	char *ubootpartname = CONFIG_UBOOT_PARTITION;
	char *partname;
	u8 pnum;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Initialize partitions */
	if (mtdparts_init()) {
		printf("Cannot initialize MTD partitions\n");
		return -1;
	}

	/* Get data of partition to be updated (might be a reserved name) */
	if (!strcmp(argv[1], "uboot"))
		partname = ubootpartname;
	else
		partname = argv[1];

	if (find_dev_and_part(partname, &dev, &pnum, &part)) {
		printf("Cannot find '%s' partition\n", partname);
		return -1;
	}

	if (dev->id->type != MTD_DEV_TYPE_NAND) {
		printf("not a NAND device\n");
		return -1;
	}

	/* Ask for confirmation if needed */
	if (getenv_yesno("forced_update") != 1) {
		/* Confirm programming */
		if (!strcmp(part->name, CONFIG_UBOOT_PARTITION) &&
		    !confirm_msg("Do you really want to program "
				 "the boot loader? <y/N> "))
			return CMD_RET_FAILURE;
	}

	/* Get source of update firmware file */
	if (argc > 2) {
		src = get_source(argc, argv, &devpartno, &fs);
		if (src == SRC_UNSUPPORTED) {
			printf("Error: '%s' is not supported as source\n",
				argv[2]);
			return CMD_RET_USAGE;
		}
		else if (src == SRC_UNDEFINED) {
			printf("Error: undefined source\n");
			return CMD_RET_USAGE;
		}
	}

	loadaddr = getenv_ulong("loadaddr", 16, CONFIG_LOADADDR);
	/*
	 * If undefined, calculate 'verifyaddr' as halfway through the RAM
	 * from $loadaddr.
	 */
	if (NULL == getenv("verifyaddr")) {
		verifyaddr = loadaddr +
			     ((gd->ram_size - (loadaddr - PHYS_SDRAM)) / 2);
		if (verifyaddr > loadaddr &&
		    verifyaddr < (PHYS_SDRAM + gd->ram_size))
			setenv_hex("verifyaddr", verifyaddr);
	}

	if (src == SRC_RAM) {
		/* Get address in RAM where firmware file is */
		if (argc > 3)
			loadaddr = simple_strtol(argv[3], NULL, 16);

		/* Get filesize */
		if (argc > 4)
			filesize = simple_strtol(argv[4], NULL, 16);
	} else {
		/*
		 * TODO: If otf-update is undefined, check if there is enough
		 * RAM to hold the largest possible file that fits into
		 * the destiny partition.
		 */

		/* Get firmware file name */
		ret = get_fw_filename(argc, argv, src, filename);
		if (ret) {
			/* Filename was not provided. Look for default one */
			ret = get_default_filename(argv[1], filename, CMD_UPDATE);
			if (ret) {
				printf("Error: need a filename\n");
				return CMD_RET_USAGE;
			}
		}
	}

	/* TODO: Activate on-the-fly update if needed */

	if (src != SRC_RAM) {
		/*
		 * Load firmware file to RAM (this process may write the file
		 * to the target media if OTF mechanism is enabled).
		 */
		ret = load_firmware(src, filename, devpartno, fs, "$loadaddr",
				    NULL);
		if (ret == LDFW_ERROR) {
			printf("Error loading firmware file to RAM\n");
			ret = CMD_RET_FAILURE;
			goto _ret;
		} else if (ret == LDFW_LOADED && otf) {
			ret = CMD_RET_SUCCESS;
			goto _ret;
		}

	}

	/* Write firmware file from RAM to storage */
	if (!filesize)
		filesize = getenv_ulong("filesize", 16, 0);
	ret = write_firmware(loadaddr, filesize, part);
	if (ret) {
		if (ret == ERR_READ)
			printf("Error while reading back written firmware!\n");
		else if (ret == ERR_VERIFY)
			printf("Error while verifying written firmware!\n");
		else
			printf("Error writing firmware!\n");
		ret = CMD_RET_FAILURE;
		goto _ret;
	}

_ret:
	return ret;
}

U_BOOT_CMD(
	update,	6,	0,	do_update,
	"Digi modules update command",
	"<partition>  [source] [extra-args...]\n"
	" Description: updates (raw writes) <partition> in NAND via <source>\n"
	" Arguments:\n"
	"   - partition:    a partition index, a GUID partition name, or one\n"
	"                   of the reserved names: uboot\n"
	"   - [source]:     " CONFIG_UPDATE_SUPPORTED_SOURCES_LIST "\n"
	"   - [extra-args]: extra arguments depending on 'source'\n"
	"\n"
	CONFIG_UPDATE_SUPPORTED_SOURCES_ARGS_HELP
);
