/*
 *  Copyright (C) 2014-2020 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <asm/global_data.h>
#include <command.h>
#include <common.h>
#include <env.h>
#ifdef CONFIG_FASTBOOT_FLASH
#include <image-sparse.h>
#endif
#include <mmc.h>
#include <otf_update.h>
#include <part.h>
#include "../board/digi/common/helper.h"

#ifdef CONFIG_FSL_FASTBOOT
#include <fb_fsl.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

static struct blk_desc *mmc_dev;
static int mmc_dev_index;

extern int mmc_get_bootdevindex(void);
extern int update_chunk(otf_data_t *oftd);
extern void register_tftp_otf_update_hook(int (*hook)(otf_data_t *oftd),
					  struct disk_partition*);
extern void unregister_tftp_otf_update_hook(void);
extern void register_fs_otf_update_hook(int (*hook)(otf_data_t *oftd),
					struct disk_partition*);
extern void unregister_fs_otf_update_hook(void);

int register_otf_hook(int src, int (*hook)(otf_data_t *oftd),
		       struct disk_partition *partition)
{
	switch (src) {
	case SRC_TFTP:
		register_tftp_otf_update_hook(hook, partition);
		return 1;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
		register_fs_otf_update_hook(hook, partition);
		return 1;
	case SRC_NFS:
		//TODO
		break;
	}

	return 0;
}

void unregister_otf_hook(int src)
{
	switch (src) {
	case SRC_TFTP:
		unregister_tftp_otf_update_hook();
		break;
	case SRC_MMC:
		unregister_fs_otf_update_hook();
		break;
	case SRC_USB:
	case SRC_SATA:
	case SRC_NFS:
		//TODO
		break;
	}

}

#ifdef CONFIG_FASTBOOT_FLASH
struct fb_mmc_sparse {
	struct blk_desc	*dev_desc;
};

static lbaint_t fb_mmc_sparse_write(struct sparse_storage *info, lbaint_t blk,
				    lbaint_t blkcnt, const void *buffer)
{
	struct fb_mmc_sparse *sparse = info->priv;

	return blk_dwrite(sparse->dev_desc, blk, blkcnt, buffer);
}

static lbaint_t fb_mmc_sparse_reserve(struct sparse_storage *info,
				      lbaint_t blk, lbaint_t blkcnt)
{
	return blkcnt;
}
#endif

enum {
	ERR_WRITE = 1,
	ERR_READ,
	ERR_VERIFY,
};

static int write_firmware(char *partname, unsigned long loadaddr,
			  unsigned long filesize, struct disk_partition *info)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	unsigned long size_blks, verifyaddr, u, m;

#ifdef CONFIG_FASTBOOT_FLASH
	if (is_sparse_image((void *)loadaddr)) {
		struct fb_mmc_sparse sparse_priv = {
			.dev_desc = mmc_dev,
		};
		struct sparse_storage sparse = {
			.blksz = mmc_dev->blksz,
			.start = info->start,
			.size = info->size,
			.write = fb_mmc_sparse_write,
			.reserve = fb_mmc_sparse_reserve,
			.priv = &sparse_priv,
		};

		return write_sparse_image(&sparse, partname, (void *)loadaddr,
					  NULL) ? ERR_WRITE : 0;
	}
#endif
	size_blks = (filesize / mmc_dev->blksz) + (filesize % mmc_dev->blksz != 0);

	if (size_blks > info->size) {
		printf("File size (%lu bytes) exceeds partition size (%lu bytes)!\n",
			filesize,
			info->size * mmc_dev->blksz);
		return -1;
	}

	/* Prepare command to change to storage device */
	sprintf(cmd, "%s dev %d", CONFIG_SYS_STORAGE_MEDIA, mmc_dev_index);

	/*
	 * If updating bootloader on eMMC
	 * append the hardware partition where bootloader lives.
	 */
	if (!strcmp(CONFIG_SYS_STORAGE_MEDIA, "mmc") &&
	    board_has_emmc() && (mmc_dev_index == CONFIG_SYS_MMC_ENV_DEV)) {
		if (!strcmp(partname, "boot1"))
			strcat(cmd, " $mmcbootpart");
		else if (!strcmp(partname, "boot2"))
			strcat(cmd, " 2");
	}

#ifdef CONFIG_FSL_FASTBOOT
	if (!strcmp(partname, "gpt")) {
		fastboot_process_flash(partname, (void *)loadaddr, filesize,
				       cmd);

		/* Verify doesn't work, therefore we return here... */
		return strncmp(cmd, "OKAY", 4) ? ERR_WRITE : 0;
	}
#endif

	/* Change to storage device */
	if (run_command(cmd, 0)) {
		debug("Cannot change to storage device\n");
		return -1;
	}

	/* Write firmware command */
	sprintf(cmd, "%s write %lx %lx %lx", CONFIG_SYS_STORAGE_MEDIA,
		loadaddr, info->start, size_blks);
	if (run_command(cmd, 0))
		return ERR_WRITE;

	/* If there is enough RAM to hold two copies of the firmware,
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

	/* ($loadaddr + firmware size) must not exceed $verifyaddr
	 * ($verifyaddr + firmware size) must not exceed U
	 */
	if ((loadaddr + size_blks * mmc_dev->blksz) < verifyaddr &&
	    (verifyaddr + size_blks * mmc_dev->blksz) < u) {
		unsigned long filesize_padded;
		int i;

		/* Read back data... */
		printf("Reading back firmware...\n");
		sprintf(cmd, "%s read %lx %lx %lx", CONFIG_SYS_STORAGE_MEDIA,
			verifyaddr, info->start, size_blks);
		if (run_command(cmd, 0))
			return ERR_READ;
		/* ...then compare by 32-bit words (faster than by bytes)
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

static int write_file(unsigned long loadaddr, unsigned long filesize,
		      char *targetfilename, char *targetfs, int part)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";

	/* Change to storage device */
	sprintf(cmd, "%s dev %d", CONFIG_SYS_STORAGE_MEDIA, mmc_dev_index);
	if (run_command(cmd, 0)) {
		debug("Cannot change to storage device\n");
		return -1;
	}

	/* Prepare write command */
	sprintf(cmd, "%swrite %s %d:%d %lx %s %lx", targetfs,
		CONFIG_SYS_STORAGE_MEDIA, mmc_dev_index, part,
		loadaddr, targetfilename, filesize);

	return run_command(cmd, 0);
}

static int emmc_bootselect(void)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";

	/* Select boot partition and enable boot acknowledge */
	sprintf(cmd, "mmc partconf %x %x %x %x", EMMC_BOOT_DEV, EMMC_BOOT_ACK,
		EMMC_BOOT_PART, EMMC_BOOT_PART);

	return run_command(cmd, 0);
}

/*
 * This function returns the size of available RAM holding a firmware transfer.
 * This size depends on:
 *   - The total RAM available
 *   - The loadaddr
 *   - The RAM occupied by U-Boot and its location
 */
static unsigned int get_available_ram_for_update(void)
{
	unsigned int loadaddr;
	unsigned int la_off;

	loadaddr = env_get_ulong("update_addr", 16, CONFIG_DIGI_UPDATE_ADDR );
	la_off = loadaddr - gd->bd->bi_dram[0].start;

	return (gd->bd->bi_dram[0].size - CONFIG_UBOOT_RESERVED - la_off);
}

static int init_mmc_globals(void)
{
	/* Use the device in $mmcdev or else, the boot media */
	mmc_dev_index = env_get_ulong("mmcdev", 16, mmc_get_bootdevindex());
	mmc_dev = blk_get_devnum_by_type(IF_TYPE_MMC, mmc_dev_index);
	if (NULL == mmc_dev) {
		debug("Cannot determine sys storage device\n");
		return -1;
	}

	return 0;
}

__weak void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
					    struct disk_partition *info)
{
	struct mmc *mmc = find_mmc_device(EMMC_BOOT_DEV);

	info->start = EMMC_BOOT_PART_OFFSET / mmc_dev->blksz;
	/* Boot partition size - Start of boot image */
	info->size = (mmc->capacity_boot / mmc_dev->blksz) - info->start;
}

static int do_update(struct cmd_tbl* cmdtp, int flag, int argc, char * const argv[])
{
	struct disk_partition info;
	int ret;
	int otf = 0;
	int otf_enabled = 0;
	char cmd[CONFIG_SYS_CBSIZE] = "";
	unsigned long loadaddr;
	unsigned long filesize = 0;
	struct load_fw fwinfo;
	char *partname;
	char str[10];

	if (argc < 2)
		return CMD_RET_USAGE;

	memset(&fwinfo, 0, sizeof(fwinfo));

	if (init_mmc_globals())
		return CMD_RET_FAILURE;

	/* Get data of partition to be updated */
	if (!strcmp(argv[1], "boot1") ||
	    !strcmp(argv[1], "boot2")) {
		/* Simulate partition data for Boot area */
		calculate_uboot_update_settings(mmc_dev, &info);
		strcpy((char *)info.name, argv[1]);
#ifdef CONFIG_FSL_FASTBOOT
	} else if (!strcmp(argv[1], "gpt")) {
		/*
		 * gpt is not a 'real' partition, just the key-word used to
		 * program the partition table from a file. We initialize the
		 * info structure just to satisfy the sanity checks performed
		 * later to check that the file fits into the partition.
		 *
		 * Partition image file generated by "bpttool" is 33.5KB
		 */
		info.start = 0;
		info.size = (34 * SZ_1K) / mmc_dev->blksz;
		strcpy((char *)info.name, argv[1]);
#endif
	} else {
		/* Not a reserved name. Must be a partition name or index */
		char dev_index_str[2];

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

		/* Look up partition on the device */
		sprintf(dev_index_str, "%d", mmc_dev_index);
		if (get_partition_bynameorindex(CONFIG_SYS_STORAGE_MEDIA,
					dev_index_str, partname, &info) < 0) {
			printf("Error: partition '%s' not found\n", argv[1]);
			return CMD_RET_FAILURE;
		}
	}

	/* Ask for confirmation if needed */
	if (env_get_yesno("forced_update") != 1) {
		/* Confirm programming */
		if (!proceed_with_update((char *)info.name))
			return CMD_RET_FAILURE;
	}

	/* Get source of update firmware file */
	if (get_source(argc, argv, &fwinfo))
		return CMD_RET_FAILURE;

	loadaddr = env_get_ulong("update_addr", 16, CONFIG_DIGI_UPDATE_ADDR );
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
		/* Get firmware file name */
		ret = get_fw_filename(argc, argv, &fwinfo);
		if (ret) {
			/* Filename was not provided. Look for default one */
			strncpy(fwinfo.filename,
				get_default_filename(argv[1], CMD_UPDATE),
				sizeof(fwinfo.filename));
			if (strlen(fwinfo.filename) == 0) {
				printf("Error: need a filename\n");
				return CMD_RET_USAGE;
			}
		}

		if (env_get_yesno("otf-update") == -1) {
			/*
			 * If otf-update is undefined, check if there is enough
			 * RAM to hold the image being updated. If that is not
			 * possible, check assuming the largest possible file
			 * that fits into the destiny partition.
			 */
			unsigned long avail = get_available_ram_for_update();
			unsigned long size = get_firmware_size(&fwinfo);

			/*
			 * If it was not possible to get the file size, assume
			 * the largest possible file size (that is, the
			 * partition size).
			 */
			if (!size)
				size = info.size * mmc_dev->blksz;

			if (avail <= size) {
				printf("Partition to update is larger (%d MiB) than the\n"
				       "available RAM memory (%d MiB, starting at $update_addr=0x%08x).\n",
				       (int)(info.size * mmc_dev->blksz / (1024 * 1024)),
				       (int)(avail / (1024 * 1024)),
				       (unsigned int)loadaddr);
				printf("Activating On-the-fly update mechanism.\n");
				otf_enabled = 1;
			}
		}
	}

	/* Activate on-the-fly update if needed */
	if (otf_enabled || (env_get_yesno("otf-update") == 1)) {
		if (!strcmp((char *)info.name, "boot1") ||
		    !strcmp((char *)info.name, "boot2")) {
			/* Do not activate on-the-fly update for Boot area */
			printf("On-the-fly mechanism disabled for Boot area "
				"for security reasons\n");
		} else {
			/* register on-the-fly update mechanism */
			otf = register_otf_hook(fwinfo.src, update_chunk,
						&info);
		}
	}

	if (otf) {
		/* Prepare command to change to storage device */
		sprintf(cmd, CONFIG_SYS_STORAGE_MEDIA " dev %d", mmc_dev_index);
		/* Change to storage device */
		if (run_command(cmd, 0)) {
			printf("Error: cannot change to storage device\n");
			ret = CMD_RET_FAILURE;
			goto _ret;
		}
	}

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
	ret = write_firmware(argv[1], loadaddr, filesize, &info);
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

	/*
	 * If updating Bootloader into eMMC, instruct the eMMC to boot from
	 * special hardware partition.
	 */
	if (!strcmp(argv[1], "boot1") &&
	    !strcmp(CONFIG_SYS_STORAGE_MEDIA, "mmc") &&
	    board_has_emmc() && (mmc_dev_index == CONFIG_SYS_MMC_ENV_DEV)) {
		ret = emmc_bootselect();
		if (ret) {
			printf("Error changing eMMC boot partition\n");
			ret = CMD_RET_FAILURE;
			goto _ret;
		}
	}

_ret:
	unregister_otf_hook(fwinfo.src);
	return ret;
}

U_BOOT_CMD(
	update,	6,	0,	do_update,
	"Digi modules update command",
	"<partition>  [source] [extra-args...]\n"
	" Description: updates (raw writes) <partition> in $mmcdev via <source>\n"
	" Arguments:\n"
	"   - partition:    a partition index, a GUID partition name, or one\n"
	"                   of the reserved names: boot1, boot2, gpt\n"
	"                   - 'boot1' to update the bootloader.'\n"
	"                   - 'boot2' to update the redundant bootloader.'\n"
	"   - [source]:     " CONFIG_UPDATE_SUPPORTED_SOURCES_LIST "\n"
	"   - [extra-args]: extra arguments depending on 'source'\n"
	"\n"
	CONFIG_UPDATE_SUPPORTED_SOURCES_ARGS_HELP
);

/* Certain command line arguments of 'update' command may be at different
 * index depending on the selected <source>. This function returns in 'arg'
 * the argument at <index> plus an offset that depends on the selected <source>
 * Upon calling, the <index> must be given as if <source> was SRC_RAM.
 */
static int get_arg_src(int argc, char * const argv[], int src, int index,
		       char **arg)
{
	switch (src) {
	case SRC_TFTP:
	case SRC_NFS:
		index += 1;
		break;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
		/*
		 * For backwards compatibility, check if old 'fs' parameter
		 * was passed before the filename.
		 */
		if ((argc > 5) &&
		    (!strcmp(argv[4], "fat") || !strcmp(argv[4], "ext4")))
			index += 3;
		else
			index += 2;
		break;
	case SRC_RAM:
		/* 2-(7-argc) */
		index += argc - 5;
		break;
	default:
		return -1;
	}

	if (argc > index) {
		*arg = (char *)argv[index];

		return 0;
	}

	return -1;
}

static int do_updatefile(struct cmd_tbl* cmdtp, int flag, int argc,
			 char * const argv[])
{
	struct disk_partition info;
	char *targetfilename = NULL;
	char *targetfs = NULL;
	const char *default_fs = "fat";
	int part;
	int i;
	char *supported_fs[] = {
#ifdef CONFIG_FAT_WRITE
		"fat",
#endif
#ifdef CONFIG_EXT4_WRITE
		"ext4",
#endif
	};
	char dev_index_str[2];
	unsigned long loadaddr;
	unsigned long filesize = 0;
	struct load_fw fwinfo;
	char *str;

	if (argc < 2)
		return CMD_RET_USAGE;

	memset(&fwinfo, 0, sizeof(fwinfo));

	if (init_mmc_globals())
		return CMD_RET_FAILURE;

	/* Get data of partition to be updated */
	sprintf(dev_index_str, "%d", mmc_dev_index);
	part = get_partition_bynameorindex(CONFIG_SYS_STORAGE_MEDIA,
					   dev_index_str, argv[1], &info);
	if (part < 0) {
		printf("Error: partition '%s' not found\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	/* Get source of update firmware file */
	if (get_source(argc, argv, &fwinfo))
		return CMD_RET_FAILURE;

	loadaddr = env_get_ulong("update_addr", 16, CONFIG_DIGI_UPDATE_ADDR);

	if (fwinfo.src == SRC_RAM) {
		/* Get address in RAM where file is */
		if (argc > 5)
			loadaddr = simple_strtol(argv[3], NULL, 16);

		/* Get filesize */
		if (argc > 6)
			filesize = simple_strtol(argv[4], NULL, 16);
	} else {
		/* Get file name */
		if (get_arg_src(argc, argv, fwinfo.src, 2, &str)) {
			printf("Error: need a filename\n");
			return CMD_RET_USAGE;
		}
		strncpy(fwinfo.filename, str, sizeof(fwinfo.filename));
	}

	/* Get target file name. If not provided use fwinfo.filename by default */
	if (get_arg_src(argc, argv, fwinfo.src, 3, &targetfilename))
		targetfilename = fwinfo.filename;

	/* Get target filesystem. If not provided use 'fat' by default */
	if (get_arg_src(argc, argv, fwinfo.src, 4, &targetfs))
		targetfs = (char *)default_fs;

	/* Check target fs is supported */
	for (i = 0; i < ARRAY_SIZE(supported_fs); i++)
		if (!strcmp(targetfs, supported_fs[i]))
			break;

	if (i >= ARRAY_SIZE(supported_fs)) {
		printf("Error: target file system '%s' is unsupported for "
			"write operation.\n"
			"Valid file systems are: ", targetfs);
		for (i = 0; i < ARRAY_SIZE(supported_fs); i++)
			printf("%s ", supported_fs[i]);
		printf("\n");
		return CMD_RET_FAILURE;
	}

	/* Load firmware file to RAM */
	strcpy(fwinfo.loadaddr, "$update_addr");

	if (LDFW_ERROR == load_firmware(&fwinfo, NULL)) {
		printf("Error loading firmware file to RAM\n");
		return CMD_RET_FAILURE;
	}

	if (!filesize)
		filesize = env_get_ulong("filesize", 16, 0);

	/* Write file from RAM to storage partition */
	if (write_file(loadaddr, filesize, targetfilename, targetfs, part)) {
		printf("Error writing file\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	updatefile,	8,	0,	do_updatefile,
	"Digi modules updatefile command",
	"<partition>  [source] [extra-args...]\n"
	" Description: updates/writes a file in <partition> in $mmcdev via\n"
	"              <source>\n"
	" Arguments:\n"
	"   - partition:    a partition index or a GUID partition name where\n"
	"                   to upload the file\n"
	"   - [source]:     " CONFIG_UPDATE_SUPPORTED_SOURCES_LIST "\n"
	"   - [extra-args]: extra arguments depending on 'source'\n"
	"\n"
	CONFIG_UPDATEFILE_SUPPORTED_SOURCES_ARGS_HELP
);
