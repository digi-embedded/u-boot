/*
 *  Copyright (C) 2016-2023 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <asm/global_data.h>
#ifdef DIGI_IMX_FAMILY
#include <asm/mach-imx/boot_mode.h>
#endif
#include <command.h>
#include <common.h>
#include <env.h>
#include <jffs2/load_kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <mapmem.h>
#include <nand.h>
#include <otf_update.h>
#include <ubi_uboot.h>

#include "../board/digi/common/helper.h"
#ifdef CONFIG_CMD_BOOTSTREAM
#include "../board/digi/common/cmd_bootstream/cmd_bootstream.h"
#endif

DECLARE_GLOBAL_DATA_PTR;

enum {
	ERR_WRITE = 1,
	ERR_READ,
	ERR_VERIFY,
};

/*
 * Attaches an MTD partition to UBI and gets its volume name.
 * If a volume does not exist, it creates one with the same name of the
 * partition.
 */
static int ubi_attach_getcreatevol(char *partname, const char **volname)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	int ret = -1;

	*volname = NULL;

	/* Attach partition and get volume name */
	if (!ubi_part(partname, NULL)) {
		*volname = ubi_get_volume_name(0);
		if (!*volname) {
			/* Create UBI volume with the name of the partition */
			sprintf(cmd, "ubi createvol %s", partname);
			if (run_command(cmd, 0))
				return -1;

			*volname = partname;
		}
		ret = 0;
	}

	return ret;
}

static int write_firmware_ubi(unsigned long loadaddr, unsigned long filesize,
			      struct part_info *part, char *ubivolname)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";

	/* If it is a UBIFS file system, verify it using a special function */
	if (!ubivolname)
		return ERR_WRITE;

	/* A UBI volume exists in the partition, use 'ubi write' */
	sprintf(cmd, "ubi write %lx %s %lx", loadaddr, ubivolname, filesize);
	if (run_command(cmd, 0))
		return ERR_WRITE;

	printf("Verifying firmware...\n");
	if (ubi_volume_verify(ubivolname, (char *)loadaddr, 0,
				filesize, 0))
		return ERR_VERIFY;

	printf("Update was successful\n");

	return 0;
}

static int write_firmware(unsigned long loadaddr, unsigned long filesize,
			  struct part_info *part, bool force_erase)
{
	uint32_t *magic;
	const char *ubivolname = NULL;
	char cmd[CONFIG_SYS_CBSIZE] = "";
	unsigned long verifyaddr, u, m;

	if (filesize > part->size) {
		printf("File size (%lu bytes) exceeds partition size (%lu bytes)!\n",
			filesize, (unsigned long)part->size);
		return -1;
	}

#ifdef CONFIG_DIGI_UBI
	/* Check if the file to write is UBIFS or SQUASHFS */
	magic = (uint32_t *)map_sysmem(loadaddr, 0);
	if (*magic == UBIFS_MAGIC || *magic == SQUASHFS_MAGIC) {
		/*
		 * If the partition is not U-Boot (whose sectors must be raw-read),
		 * ensure the partition is UBI formatted.
		 */
		if (strcmp(part->name, UBOOT_PARTITION)) {
			/* Silent UBI commands during the update */
			run_command("ubi silent 1", 0);

			if (!force_erase && is_ubi_partition(part)) {
				/* Attach partition and get volume name */
				ubi_attach_getcreatevol(part->name, &ubivolname);
			}

			/*
			 * If the partition does not have a valid UBI volume,
			 * or we forced it from command line, erase the
			 * partition and create the UBI volume.
			 */
			if (!ubivolname || force_erase) {
				sprintf(cmd, "nand erase.part %s", part->name);
				if (run_command(cmd, 0))
					return ERR_WRITE;
				/* Attach partition and get volume name */
				ubi_attach_getcreatevol(part->name, &ubivolname);
			}
		}
	} else {
		/*
		 * If the file is not UBIFS, erase the entire partition before
		 * raw-writing.
		 */
		sprintf(cmd, "nand erase.part %s", part->name);
		if (run_command(cmd, 0))
			return ERR_WRITE;
	}
#endif /* CONFIG_DIGI_UBI */

	if (ubivolname) {
		/* A UBI volume exists in the partition, use 'ubi write' */
		sprintf(cmd, "ubi write %lx %s %lx", loadaddr, ubivolname,
			filesize);
	} else {
		/* raw-write firmware command */
		sprintf(cmd, "nand write %lx %s %lx", loadaddr, part->name,
			filesize);
	}
	if (run_command(cmd, 0))
		return ERR_WRITE;

#ifdef CONFIG_DIGI_UBI
	/* If it is a UBIFS file system, verify it using a special function */
	if (ubivolname) {
		printf("Verifying firmware...\n");
		if (ubi_volume_verify(part->name, (char *)loadaddr, 0,
				      filesize, 0))
			return ERR_VERIFY;
		printf("Update was successful\n");

		return 0;
	}
#endif

	/*
	 * If there is enough RAM to hold two copies of the firmware,
	 * verify written firmware.
	 * +--------|---------------------|------------------|--------------+
	 * |        L                     V                  | U-Boot+Stack |
	 * +--------|---------------------|------------------|--------------+
	 * P                                                 U              M
	 *
	 *  P = CONFIG_SYS_SDRAM_BASE (base address of SDRAM)
	 *  L = $loadaddr
	 *  V = $verifyaddr
	 *  M = last address of SDRAM ((size of SDRAM) + P)
	 *  U = SDRAM address where U-Boot is located (plus margin)
	 */
	verifyaddr = env_get_ulong("verifyaddr", 16, 0);
	m = CONFIG_SYS_SDRAM_BASE + gd->ram_size;
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

static int do_update(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	int otf = 0;
	unsigned long loadaddr;
	unsigned long filesize = 0;
	struct mtd_device *dev;
	struct part_info *part;
	char *ubootpartname = UBOOT_PARTITION;
	char *partname;
	u8 pnum;
	struct load_fw fwinfo;
	char cmd[CONFIG_SYS_CBSIZE];
	bool ubivol = false;
	bool force_erase = false;
	char str[20];
	char syspart[20];
	int i;

	if (argc < 2)
		return CMD_RET_USAGE;

	memset(&fwinfo, 0, sizeof(fwinfo));

	/* Check optional '-e' switch (at the end only) */
	if (!strcmp(argv[argc - 1], "-e")) {
		force_erase = true;
		/* Drop off the '-e' argument */
		argc--;
	}

	/* Initialize partitions */
	if (mtdparts_init()) {
		printf("Cannot initialize MTD partitions\n");
		return -1;
	}

	/* Get data of partition to be updated (might be a reserved name) */
	if (!strcmp(argv[1], "uboot")) {
		partname = ubootpartname;
	} else {
		/*
		 * If dualboot is enabled and 'partname' is "linux" or "rootfs"
		 * check the active partition name (linux_a/linux_b or
		 * rootfs_a/rootfs_b).
		 */
		partname = argv[1];
		if (env_get_yesno("dualboot")) {
			strcpy(str, env_get("active_system"));
			if (!strcmp(partname, "linux")) {
				partname = str;
			} else if (!strcmp(partname, "rootfs")) {
				strcpy(str, strcmp(str, "linux_a") ?
				       "rootfs_b" : "rootfs_a");
				partname = str;
			}
		}
	}

	if (find_dev_and_part(partname, &dev, &pnum, &part)) {
		/*
		 * Check if the passed argument is a UBI volume in any of
		 * the system partitions.
		 */
		for (i = 0; i < NUM_SYSTEM_PARTITIONS; i++) {
			if (i > 0)
				sprintf(syspart, SYSTEM_PARTITION "_%d", i + 1);
			else
				strcpy(syspart, SYSTEM_PARTITION);
			if (find_dev_and_part(syspart, &dev, &pnum,
						&part)) {
				printf("Cannot find '%s' partition or UBI volume\n",
					partname);
				return -1;
			}
			sprintf(cmd, "ubi part %s", syspart);
			if (run_command(cmd, 0)) {
				printf("Cannot find '%s' partition or UBI volume\n",
					partname);
				return -1;
			}
			sprintf(cmd, "ubi check %s", partname);
			if (run_command(cmd, 0))
				continue;
			ubivol = true;
			break;
		}
		if (!ubivol) {
			printf("Cannot find '%s' partition or UBI volume\n",
				partname);
			return -1;
		}
	} else {
		if (dev->id->type != MTD_DEV_TYPE_NAND) {
			printf("not a NAND device\n");
			return -1;
		}
	}

	/* Ask for confirmation if needed */
	if (env_get_yesno("forced_update") != 1) {
		/* Confirm programming */
		if (!proceed_with_update(part->name))
			return CMD_RET_FAILURE;
	}

	/* Get source of update firmware file */
	if (get_source(argc, argv, &fwinfo))
		return CMD_RET_FAILURE;

	loadaddr = env_get_ulong("update_addr", 16, CONFIG_DIGI_UPDATE_ADDR);
	/* If undefined, calculate 'verifyaddr' */
	if (NULL == env_get("verifyaddr"))
		set_verifyaddr(loadaddr);

	if (fwinfo.src == SRC_RAM) {
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
		ret = get_fw_filename(argc, argv, &fwinfo);
		if (ret) {
			strncpy(fwinfo.filename,
				get_default_filename(argv[1], CMD_UPDATE),
				sizeof(fwinfo.filename));
			if (strlen(fwinfo.filename) == 0) {
				printf("Error: need a filename\n");
				return CMD_RET_USAGE;
			}
		}
	}

	/* TODO: Activate on-the-fly update if needed */

	if (fwinfo.src != SRC_RAM) {
		/*
		 * Load firmware file to RAM (this process may write the file
		 * to the target media if OTF mechanism is enabled).
		 */
		strcpy(fwinfo.loadaddr, "$update_addr");
		ret = load_firmware(&fwinfo, NULL);
		if (ret == LDFW_ERROR) {
			printf("Error loading firmware file to RAM\n");
			ret = CMD_RET_FAILURE;
			goto _ret;
		} else if (ret == LDFW_LOADED && otf) {
			ret = CMD_RET_SUCCESS;
			goto _ret;
		}

	}

	/* Validate U-Boot updates, to avoid writing the wrong image to flash */
	if (!strcmp(argv[1], "uboot") &&
	    env_get_yesno("skip-uboot-check") != 1 &&
	    !validate_bootloader_image((void *)loadaddr)) {
		printf("ERROR: Aborting update operation.\n"
		       "To skip U-Boot validation, set variable 'skip-uboot-check' to 'yes' and repeat the update command.\n");
		ret = CMD_RET_FAILURE;
		goto _ret;
	}

	/* Write firmware file from RAM to storage */
	if (!filesize)
		filesize = env_get_ulong("filesize", 16, 0);
#ifdef CONFIG_CMD_BOOTSTREAM
	/* U-Boot is written in a special way */
	if (!strcmp(partname, UBOOT_PARTITION)) {
		ret = write_bootstream(part, loadaddr, filesize);
		goto _ret;
	}
#endif
	if (ubivol)
		ret = write_firmware_ubi(loadaddr, filesize, part, partname);
	else
		ret = write_firmware(loadaddr, filesize, part, force_erase);

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
#ifdef CONFIG_DIGI_UBI
	/* restore UBI commands verbosity */
	run_command("ubi silent 0", 0);
#endif

	return ret;
}

U_BOOT_CMD(
	update,	6,	0,	do_update,
	"Digi modules update command",
	"<partition>  [source] [extra-args...] [-e]\n"
	" Description: updates <partition> in NAND via <source>\n"
	"              If a UBIFS file is given, it writes the file to the partition using UBI commands.\n"
	"              Otherwise, this command raw-writes the file to the partition.\n"
	"\n"
	" Arguments:\n"
	"   - partition:    a partition index, a GUID partition name, or one\n"
	"                   of the reserved names: uboot\n"
	"   - [source]:     " CONFIG_UPDATE_SUPPORTED_SOURCES_LIST "\n"
	"   - [extra-args]: extra arguments depending on 'source'\n"
	"\n"
	CONFIG_UPDATE_SUPPORTED_SOURCES_ARGS_HELP
	"\n"
	"   - [-e]:         force erase of the MTD partition before update\n"
);
