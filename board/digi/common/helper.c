/*
 *  Copyright (C) 2017 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <asm/global_data.h>
#include <command.h>
#include <common.h>
#include <console.h>
#include <env.h>
#include <gzip.h>
#include <linux/errno.h>
#include <fsl_sec.h>
#include <asm/mach-imx/hab.h>
#include <malloc.h>
#include <mapmem.h>
#include <nand.h>
#include <ubi_uboot.h>
#include <version_string.h>
#include <watchdog.h>
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif
#include <otf_update.h>
#include "helper.h"
#include "hwid.h"
#ifdef CONFIG_FSL_CAAM
#include "../drivers/crypto/fsl/jr.h"
#endif
#ifdef CONFIG_ANDROID_SUPPORT
#include "mca.h"
#include "mca_registers.h"
#endif

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
#define SQUASHFS_BYTES_USED_OFFSET	0x28
#ifdef CONFIG_AHAB_BOOT
#define AHAB_CONTAINER_SIZE		8192
#endif
#endif /* CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS */

#if defined(CONFIG_CMD_UPDATE) || defined(CONFIG_CMD_DBOOT)
enum {
	FWLOAD_NO,
	FWLOAD_YES,
	FWLOAD_TRY,
};

static const char *src_strings[] = {
	[SRC_TFTP] =	"tftp",
	[SRC_NFS] =	"nfs",
	[SRC_NAND] =	"nand",
	[SRC_USB] =	"usb",
	[SRC_MMC] =	"mmc",
	[SRC_RAM] =	"ram",
	[SRC_SATA] =	"sata",
};

#ifdef CONFIG_CMD_UPDATE
/* hook for on-the-fly update and register function */
static int (*otf_update_hook)(otf_data_t *data) = NULL;
/* Data struct for on-the-fly update */
static otf_data_t otfd;
#endif
#endif /* CONFIG_CMD_UPDATE || CONFIG_CMD_DBOOT */

#ifdef CONFIG_AUTO_BOOTSCRIPT
#define AUTOSCRIPT_TFTP_MSEC		100
#define AUTOSCRIPT_TFTP_CNT		15
#define AUTOSCRIPT_START_AGAIN		100
extern ulong tftp_timeout_ms;
extern int tftp_timeout_count_max;
extern unsigned long net_start_again_timeout;
int DownloadingAutoScript = 0;
int RunningAutoScript = 0;
#endif

#ifdef CONFIG_HAS_TRUSTFENCE
int rng_swtest_status = 0;
#endif

int confirm_msg(char *msg)
{
#ifdef CONFIG_AUTO_BOOTSCRIPT
        /* From autoscript we shouldn't expect user's confirmations.
         * Assume yes is the correct answer here to avoid halting the script.
         */
	if (RunningAutoScript)
		return 1;
#endif

	printf(msg);
	if (confirm_yesno())
		return 1;

	puts("Operation aborted by user\n");
	return 0;
}

#ifdef CONFIG_CMD_UPDATE
void register_fs_otf_update_hook(int (*hook)(otf_data_t *data),
				 struct disk_partition *partition)
{
	otf_update_hook = hook;
	/* Initialize data for new transfer */
	otfd.buf = NULL;
	otfd.part = partition;
	otfd.flags = OTF_FLAG_INIT;
	otfd.offset = 0;
}

void unregister_fs_otf_update_hook(void)
{
	otf_update_hook = NULL;
}

/* On-the-fly update for files in a filesystem on mass storage media
 * The function returns:
 *	0 if the file was loaded successfully
 *	-1 on error
 */
static int write_file_fs_otf(int src, char *filename, char *devpartno)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	unsigned long filesize;
	unsigned long remaining;
	unsigned long offset = 0;

	/* Obtain file size */
	sprintf(cmd, "size %s %s %s", src_strings[src], devpartno, filename);
	if (run_command(cmd, 0)) {
		printf("Couldn't determine file size\n");
		return -1;
	}
	filesize = env_get_ulong("filesize", 16, 0);
	remaining = filesize;

	/* Init otf data */
	otfd.loadaddr = (void *)env_get_ulong("update_addr", 16, CONFIG_DIGI_UPDATE_ADDR );

	while (remaining > 0) {
		unsigned long max_length = CONFIG_OTF_CHUNK - otfd.offset;

		debug("%lu remaining bytes\n", remaining);
		/* Determine chunk length to write */
		if (remaining > max_length) {
			otfd.len = max_length;
		} else {
			otfd.flags |= OTF_FLAG_FLUSH;
			otfd.len = remaining;
		}

		/* Load 'len' bytes from file[offset] into RAM */
		sprintf(cmd, "load %s %s 0x%p %s %x %x", src_strings[src],
			devpartno, otfd.loadaddr + otfd.offset, filename,
			otfd.len, (unsigned int) offset);
		if (run_command(cmd, 0)) {
			printf("Couldn't load file\n");
			return -1;
		}

		/* Write chunk */
		if (otf_update_hook(&otfd)) {
			printf("Error writing on-the-fly. Aborting\n");
			return -1;
		}

		/* Update local target offset */
		offset += otfd.len;
		/* Update remaining bytes */
		remaining -= otfd.len;
	}

	return 0;
}
#endif /* CONFIG_CMD_UPDATE */

static int get_default_devpartno(int src, char *devpartno)
{
	char *dev, *part;

	switch (src) {
	case SRC_MMC:
		dev = env_get("mmcdev");
		if (dev == NULL)
			return -1;
		part = env_get("mmcpart");
		/* If mmcpart not defined, default to 1 */
		if (part == NULL)
			sprintf(devpartno, "%s:1", dev);
		else
			sprintf(devpartno, "%s:%s", dev, part);
		break;
	case SRC_USB:	// TODO
	case SRC_SATA:	// TODO
	default:
		return -1;
	}

	return 0;
}

#if defined(CONFIG_CMD_UPDATE) || defined(CONFIG_CMD_DBOOT)
bool is_image_compressed(void)
{
	char *var;

	var = env_get("dboot_kernel_var");
	if (var && !strcmp(var, "imagegz"))
		return true;

	return false;
}

int activate_ubi_part(char *partname)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	struct ubi_device *ubi = ubi_devices[0];

	if (ubi && !strcmp(ubi->mtd->name, partname))
		return 0;

	sprintf(cmd, "ubi part %s", partname);
	return run_command(cmd, 0);
}

int get_source(int argc, char * const argv[], struct load_fw *fwinfo)
{
	int i;
	char *src;
	char def_devpartno[] = "0:1";
#ifdef CONFIG_CMD_MTDPARTS
	struct mtd_device *dev;
	u8 pnum;
	char *partname;
#endif

	if (argc < 3) {
		fwinfo->src = SRC_TFTP;	/* default to TFTP */
		return 0;
	}

	src = argv[2];
	for (i = 0; i < ARRAY_SIZE(src_strings); i++) {
		if (!strncmp(src_strings[i], src, strlen(src))) {
			if (1 << i & CONFIG_SUPPORTED_SOURCES) {
				break;
			} else {
				fwinfo->src = SRC_UNSUPPORTED;
				goto _err;
			}
		}
	}

	if (i >= ARRAY_SIZE(src_strings)) {
		fwinfo->src = SRC_UNDEFINED;
		goto _err;
	}
	fwinfo->src = i;

	switch (fwinfo->src) {
	case SRC_USB:
	case SRC_MMC:
	case SRC_SATA:
		/* Get device:partition */
		if (argc > 3) {
			strncpy(fwinfo->devpartno, argv[3],
				sizeof(fwinfo->devpartno));
		} else {
			get_default_devpartno(fwinfo->src, def_devpartno);
			strncpy(fwinfo->devpartno, def_devpartno,
				sizeof(fwinfo->devpartno));
		}
		break;
	case SRC_NAND:
#ifdef CONFIG_CMD_MTDPARTS
		/* Initialize partitions */
		if (mtdparts_init()) {
			printf("Cannot initialize MTD partitions\n");
			goto _err;
		}

		/*
		 * Use partition name if provided, or else search for a
		 * partition with the same name as the OS.
		 */
		if (argc > 3)
			partname = argv[3];
		else
			partname = argv[1];
		if (find_dev_and_part(partname, &dev, &pnum, &fwinfo->part)) {
			char cmd[CONFIG_SYS_CBSIZE] = "";

			/*
			 * Check if the passed argument is a UBI volume in the
			 * 'system' partition.
			 */
			if (find_dev_and_part(SYSTEM_PARTITION, &dev,
						&pnum, &fwinfo->part)) {
				printf("Cannot find '%s' partition or UBI volume\n",
					partname);
				return -1;
			}
			if (activate_ubi_part(SYSTEM_PARTITION)) {
				printf("Cannot find '%s' partition or UBI volume\n",
					partname);
				return -1;
			}
			sprintf(cmd, "ubi check %s", partname);
			if (run_command(cmd, 0)) {
				printf("Cannot find '%s' partition or UBI volume\n",
					partname);
				return -1;
			}
			fwinfo->ubivol = true;
			strcpy(fwinfo->ubivolname, partname);
			goto _ok;
		}
#endif
		break;
	}

#ifdef CONFIG_CMD_MTDPARTS
_ok:
#endif
	return 0;

_err:
	if (fwinfo->src == SRC_UNSUPPORTED)
		printf("Error: '%s' is not supported as source\n", argv[2]);
	else if (fwinfo->src == SRC_UNDEFINED)
		printf("Error: undefined source\n");

	return -1;
}

const char *get_source_string(int src)
{
	if (SRC_UNDEFINED != src && src < ARRAY_SIZE(src_strings))
		return src_strings[src];

	return "";
}

int get_fw_filename(int argc, char * const argv[], struct load_fw *fwinfo)
{
	int filename_index = 4;

	switch (fwinfo->src) {
	case SRC_TFTP:
	case SRC_NFS:
		if (argc > 3) {
			strncpy(fwinfo->filename, argv[3],
				sizeof(fwinfo->filename));
			return 0;
		}
		break;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
	case SRC_NAND:
		/*
		 * For backwards compatibility, check if old 'fs' parameter
		 * was passed before the filename.
		 */
		if ((argc > 5) &&
		    (!strcmp(argv[4], "fat") || !strcmp(argv[4], "ext4")))
			filename_index++;

		if (argc > 4) {
			strncpy(fwinfo->filename, argv[filename_index],
				sizeof(fwinfo->filename));
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

char *get_default_filename(char *partname, int cmd)
{
	char *var;
	char varname[100];

	switch(cmd) {
	case CMD_DBOOT:
		if (!strcmp(partname, "linux") ||
		    !strcmp(partname, "android")) {
			var = env_get("dboot_kernel_var");
			if (var)
				return env_get(var);
			else
				return "$zimage";
		}
		break;

	case CMD_UPDATE:
		if (!strcmp(partname, "uboot")) {
			return "$uboot_file";
		} else {
			/* Read the default filename from a variable called
			 * after the partition name: <partname>_file,
			 * returning an empty string in case of partitions name is
			 * not found.
			 */
			sprintf(varname, "%s_file", partname);
			var = env_get(varname);
			if (!var)
				return "";
			else
				return var;
		}
		break;
	}

	return NULL;
}

#ifdef CONFIG_DIGI_UBI
bool is_ubi_partition(struct part_info *part)
{
	struct mtd_info *nand = get_nand_dev_by_index(0);
	size_t rsize = nand->writesize;
	unsigned char *page;
	unsigned long ubi_magic = 0x23494255;	/* "UBI#" */
	bool ret = false;

	/*
	 * Check if the partition is UBI formatted by reading the first word
	 * in the first page, which should contain the UBI magic "UBI#".
	 * Then verify it contains a UBI volume and get its name.
	 */
	page = malloc(rsize);
	if (page) {
		if (!nand_read_skip_bad(nand, part->offset, &rsize, NULL,
					part->size, page)) {
			unsigned long *magic = (unsigned long *)page;

			if (*magic == ubi_magic)
				ret = true;
		}
		free(page);
	}

	return ret;
}
#endif /* CONFIG_DIGI_UBI */

/*
 * Returns 0 if the file size cannot be obtained.
 */
unsigned long get_firmware_size(const struct load_fw *fwinfo) {
	char cmd[CONFIG_SYS_CBSIZE] = "";
	unsigned long filesize = 0;

	switch (fwinfo->src) {
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
		sprintf(cmd, "size %s %s %s", src_strings[fwinfo->src],
			fwinfo->devpartno, fwinfo->filename);

		if (!run_command(cmd, 0))
			filesize = env_get_ulong("filesize", 16, 0);

		break;
	default:
		/* leave filesize = 0 */
		break;
	}

	return filesize;
}

/* A variable determines if the file must be loaded.
 * The function returns:
 *	LDFW_LOADED if the file was loaded successfully
 *	LDFW_NOT_LOADED if the file was not loaded, but isn't required
 *	LDFW_ERROR on error
 */
int load_firmware(struct load_fw *fwinfo, char *msg)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	char def_devpartno[] = "0:1";
	int ret;
	int fwload = FWLOAD_YES;
	ulong loadaddr;

	/* 'fwinfo->varload' determines if the file must be loaded:
	 * - yes: the file must be loaded. Return error otherwise.
	 * - try: the file may be loaded. Return ok even if load fails.
	 * - no: skip the load.
	 */
	if (!strcmp(fwinfo->varload, "no"))
		return LDFW_NOT_LOADED;	/* skip load and return ok */
	else if (!strcmp(fwinfo->varload, "try"))
		fwload = FWLOAD_TRY;

	/* Use default values if not provided */
	if (strlen(fwinfo->devpartno) == 0) {
		if (get_default_devpartno(fwinfo->src, def_devpartno))
			strcpy(def_devpartno, "0:1");
		strncpy(fwinfo->devpartno, def_devpartno,
			sizeof(fwinfo->devpartno));
	}

	/*
	 * Load firmware to fwinfo->loadaddr except if loading a compressed
	 * image. Skip the leading dollar sign in the strings to obtain the
	 * value correctly
	 */
	loadaddr = fwinfo->compressed ?
	           env_get_ulong(fwinfo->lzipaddr + 1, 16, CONFIG_DIGI_LZIPADDR) :
	           env_get_ulong(fwinfo->loadaddr + 1, 16, CONFIG_DIGI_UPDATE_ADDR);

	switch (fwinfo->src) {
	case SRC_TFTP:
		sprintf(cmd, "tftpboot 0x%lx %s", loadaddr, fwinfo->filename);
		break;
	case SRC_NFS:
		sprintf(cmd, "nfs 0x%lx $rootpath/%s", loadaddr, fwinfo->filename);
		break;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
#ifdef CONFIG_CMD_UPDATE
		if (otf_update_hook) {
			ret = write_file_fs_otf(fwinfo->src, fwinfo->filename,
						fwinfo->devpartno);
			goto _ret;
		} else
#endif
		{
			sprintf(cmd, "load %s %s 0x%lx %s", src_strings[fwinfo->src],
				fwinfo->devpartno, loadaddr, fwinfo->filename);
		}
		break;
	case SRC_NAND:
#ifdef CONFIG_DIGI_UBI
		/*
		 * If the partition is UBI formatted, use 'ubiload' to read
		 * a file from the UBIFS file system. Otherwise use a raw
		 * read using 'nand read'.
		 */
		if (fwinfo->ubivol) {
			if (!activate_ubi_part(SYSTEM_PARTITION))
				sprintf(cmd,
					"if ubifsmount ubi0:%s;then "
						"ubifsload 0x%lx %s;"
#ifndef CONFIG_MTD_UBI_SKIP_REATTACH
						"ubifsumount;"
#endif
					"fi;",
					fwinfo->ubivolname,
					loadaddr, fwinfo->filename);
		} else {
			if (is_ubi_partition(fwinfo->part)) {
				if (!activate_ubi_part(fwinfo->part->name))
					sprintf(cmd,
						"if ubifsmount ubi0:%s;then "
							"ubifsload 0x%lx %s;"
#ifndef CONFIG_MTD_UBI_SKIP_REATTACH
							"ubifsumount;"
#endif
						"fi;",
					fwinfo->part->name,
					loadaddr, fwinfo->filename);
			} else {
				sprintf(cmd, "nand read %s 0x%lx %x", fwinfo->part->name,
					loadaddr, (u32)fwinfo->part->size);
			}
		}
#endif
		break;
	case SRC_RAM:
		ret = LDFW_NOT_LOADED;	/* file is already in RAM */
		goto _ret;
	default:
		return -1;
	}

	if (msg) {
		char *fn = fwinfo->filename;

		if (fwinfo->filename[0] == '$')
			fn = env_get(&fwinfo->filename[1]);
		printf("%s: %s\n", msg, fn);
	}

	ret = run_command(cmd, 0);
	if (!ret && fwinfo->compressed) {
		ulong len, src, dest;

		src = env_get_ulong(fwinfo->lzipaddr + 1, 16, 0);
		dest = env_get_ulong(fwinfo->loadaddr + 1, 16, 0);
		len = env_get_ulong("filesize", 16, 0);

		if (!src || !dest) {
			printf("ERROR: for compressed images %s and %s must be provided\n", fwinfo->lzipaddr, fwinfo->loadaddr);
			ret = -EINVAL;
		} else {
			ret = gunzip((void *)dest, INT_MAX, (uchar *)src, &len);
		}
	}

_ret:
	if (FWLOAD_TRY == fwload) {
		if (ret)
			return LDFW_NOT_LOADED;
		else
			return LDFW_LOADED;
	}

	if (ret)
		return LDFW_ERROR;

	return LDFW_LOADED;	/* ok, file was loaded */
}
#endif /* CONFIG_CMD_UPDATE || CONFIG_CMD_DBOOT */

#if defined(CONFIG_CMD_SOURCE) && defined(CONFIG_AUTO_BOOTSCRIPT)
void run_auto_bootscript(void)
{
#ifdef CONFIG_CMD_NET
	int ret;
	char *bootscript;
	/* Save original timeouts */
	ulong saved_rrqtimeout_msecs = tftp_timeout_ms;
	int saved_rrqtimeout_count = tftp_timeout_count_max;
	ulong saved_startagain_timeout = net_start_again_timeout;
	unsigned long saved_flags = gd->flags;
	char *retrycnt = env_get("netretry");

	bootscript = env_get("bootscript");
	if (bootscript) {
		printf("Bootscript from TFTP... ");

		/* Silence console */
		gd->flags |= GD_FLG_SILENT;
		/* set timeouts for bootscript */
		tftp_timeout_ms = AUTOSCRIPT_TFTP_MSEC;
		tftp_timeout_count_max = AUTOSCRIPT_TFTP_CNT;
		net_start_again_timeout = AUTOSCRIPT_START_AGAIN;
		/* set retrycnt */
		env_set("netretry", "no");

		/* Silence net commands during the bootscript download */
		DownloadingAutoScript = 1;
		ret = run_command("tftp ${loadaddr} ${bootscript}", 0);
		/* First restore original values of global variables
		 * and then evaluate the result of the run_command */
		DownloadingAutoScript = 0;
		/* Restore original timeouts */
		tftp_timeout_ms = saved_rrqtimeout_msecs;
		tftp_timeout_count_max = saved_rrqtimeout_count;
		net_start_again_timeout = saved_startagain_timeout;
		/* restore retrycnt */
		if (retrycnt)
			env_set("netretry", retrycnt);
		else
			env_set("netretry", "");
		/* Restore flags */
		gd->flags = saved_flags;

		if (ret)
			goto error;

		printf("[ready]\nRunning bootscript...\n");
		RunningAutoScript = 1;
		/* Launch bootscript */
		run_command("source ${loadaddr}", 0);
		RunningAutoScript = 0;
		return;
error:
		printf( "[not available]\n" );
	}
#endif
}
#endif

int strtou32(const char *str, unsigned int base, u32 *result)
{
	char *ep;

	*result = simple_strtoul(str, &ep, base);
	if (ep == str || *ep != '\0')
		return -EINVAL;

	return 0;
}

int confirm_prog(void)
{
	puts("Warning: Programming fuses is an irreversible operation!\n"
			"         This may brick your system.\n"
			"         Use this command only if you are sure of "
					"what you are doing!\n"
			"\nReally perform this fuse programming? <y/N>\n");

	if (getchar() == 'y') {
		int c;

		putc('y');
		c = getchar();
		putc('\n');
		if (c == '\r')
			return 1;
	}

	puts("Fuse programming aborted\n");
	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
void fdt_fixup_mac(void *fdt, char *varname, char *node, char *property)
{
	char *tmp, *end;
	unsigned char mac_addr[6];
	int i;

	if ((tmp = env_get(varname)) != NULL) {
		for (i = 0; i < 6; i++) {
			mac_addr[i] = tmp ? simple_strtoul(tmp, &end, 16) : 0;
			if (tmp)
				tmp = (*end) ? end+1 : end;
		}
		do_fixup_by_path(fdt, node, property, &mac_addr, 6, 1);
	}
}

void fdt_fixup_regulatory(void *fdt)
{
	unsigned int val;
	char *regdomain = env_get("regdomain");

	if (regdomain != NULL) {
		val = simple_strtoul(regdomain, NULL, 16);
		if (val < DIGI_MAX_CERT) {
			sprintf(regdomain, "0x%x", val);
			do_fixup_by_path(fdt, "/wireless",
					 "regulatory-domain", regdomain,
					 strlen(regdomain) + 1, 1);
		}
	}
}
#endif /* CONFIG_OF_BOARD_SETUP */

void fdt_fixup_uboot_info(void *fdt) {
	do_fixup_by_path(fdt, "/", "digi,uboot,version", version_string,
			 strlen(version_string), 1);
#ifdef CONFIG_DYNAMIC_ENV_LOCATION
	do_fixup_by_path(fdt, "/", "digi,uboot,dynamic-env", NULL, 0, 1);
#endif
}

const char *get_filename_ext(const char *filename)
{
	const char *dot;

	if (NULL == filename)
		return "";

	dot = strrchr(filename, '.');
	if (!dot || dot == filename)
		return "";

	return dot + 1;
}

/*
 * Convert string with hexadecimal characters into a hex number
 * @in: Pointer to input string
 * @out Pointer to output char array
 * @len Number of elements in the output array
*/
void strtohex(char *in, unsigned char *out, int len)
{
	char tmp[3];
	int i, j;

	for (i = 0, j = 0; j < len; i+=2, j++) {
		strncpy(tmp, &in[i], 2);
		tmp[2] = '\0';
		out[j] = simple_strtoul(tmp, NULL, 16);
	}
}

/*
 * Verifies if a MAC address has a default value (dummy) and prints a warning
 * if so.
 * @var: Variable to check
 * @default_mac: Default MAC to check with (as a string)
 */
void verify_mac_address(char *var, char *default_mac)
{
	char *mac;

	mac = env_get(var);
	if (NULL == mac)
		printf("   WARNING: MAC not set in '%s'\n", var);
	else if (!strcmp(mac, default_mac))
		printf("   WARNING: Dummy default MAC in '%s'\n", var);
}

/**
 * Parses a string into a number. The number stored at ptr is
 * potentially suffixed with K (for kilobytes, or 1024 bytes),
 * M (for megabytes, or 1048576 bytes), or G (for gigabytes, or
 * 1073741824). If the number is suffixed with K, M, or G, then
 * the return value is the number multiplied by one kilobyte, one
 * megabyte, or one gigabyte, respectively.
 *
 * @param ptr where parse begins
 * @param retptr output pointer to next char after parse completes (output)
 * @return resulting unsigned int
 */
u64 memsize_parse(const char *const ptr, const char **retptr)
{
	u64 ret = simple_strtoull(ptr, (char **)retptr, 0);

	switch (**retptr) {
	case 'G':
	case 'g':
		ret <<= 10;
	case 'M':
	case 'm':
		ret <<= 10;
	case 'K':
	case 'k':
		ret <<= 10;
		(*retptr)++;
	default:
		break;
	}

	return ret;
}

/*
 * Calculate a RAM address for verification during update command.
 * To make the better use of RAM, calculate it around half way through the
 * available RAM, but skipping reserved areas.
 */
void set_verifyaddr(unsigned long loadaddr)
{
	unsigned long verifyaddr;

	verifyaddr = loadaddr + ((gd->ram_size -
				   (loadaddr - CONFIG_SYS_SDRAM_BASE)) / 2);

	 /* Skip reserved memory area */
#if defined(RESERVED_MEM_START) && defined(RESERVED_MEM_END)
	if (verifyaddr >= RESERVED_MEM_START && verifyaddr < RESERVED_MEM_END)
		verifyaddr = RESERVED_MEM_END;
#endif

	 if (verifyaddr > loadaddr &&
	     verifyaddr < (CONFIG_SYS_SDRAM_BASE + gd->ram_size))
		 env_set_hex("verifyaddr", verifyaddr);
 }

/**
 * Validate a bootloader image in memory to see if it's apt for the board.
 * This function can be redefined per platform, to implement specific
 * validations.
 *
 * @param loadaddr pointer to the address where the bootloader has been loaded
 * @return true if the validation is successful, false otherwise
 */
__weak bool validate_bootloader_image(void *loadaddr)
{
	/* Accept all bootloaders by default */
	return true;
}

#ifdef CONFIG_HAS_TRUSTFENCE
#define RNG_FAIL_EVENT_SIZE 36

static uint8_t habv4_known_rng_fail_events[][RNG_FAIL_EVENT_SIZE] = {
	{ 0xdb, 0x00, 0x24, 0x42,  0x69, 0x30, 0xe1, 0x1d,
	  0x00, 0x80, 0x00, 0x02,  0x40, 0x00, 0x36, 0x06,
	  0x55, 0x55, 0x00, 0x03,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x01 },
	{ 0xdb, 0x00, 0x24, 0x42,  0x69, 0x30, 0xe1, 0x1d,
	  0x00, 0x04, 0x00, 0x02,  0x40, 0x00, 0x36, 0x06,
	  0x55, 0x55, 0x00, 0x03,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x01 },
};

extern enum hab_status hab_rvt_report_event(enum hab_status status, uint32_t index,
					    uint8_t *event, size_t *bytes);

int hab_event_warning_check(uint8_t *event, size_t *bytes)
{
	int ret = SW_RNG_TEST_NA, i;
	bool is_rng_fail_event = false;

	/* Get HAB Event warning data */
	hab_rvt_report_event(HAB_WARNING, 0, event, bytes);

	/* Compare HAB event warning data with known Warning issues */
	for (i = 0; i < ARRAY_SIZE(habv4_known_rng_fail_events); i++) {
		if (memcmp(event, habv4_known_rng_fail_events[i],
			   RNG_FAIL_EVENT_SIZE) == 0) {
			is_rng_fail_event = true;
			break;
		}
	}

	if (is_rng_fail_event) {
#ifdef CONFIG_RNG_SELF_TEST
		printf("RNG:   self-test failure detected, will run software self-test\n");
		rng_self_test();
		ret = SW_RNG_TEST_PASSED;
#endif
	}

	return ret;
}
#endif /* CONFIG_HAS_TRUSTFENCE */

#ifdef CONFIG_ANDROID_LOAD_CONNECTCORE_FDT
#include <dt_table.h>

/**
 * Dump information about the content of the dtbo partition
 */
void dump_fdt_part_info(struct dt_table_header *dtt_header)
{
#ifdef DEBUG_LOAD_CC_FDT
	int i;
	struct dt_table_entry *dtt_entry;
	u32 entry_count;

	dtt_entry = (struct dt_table_entry *)((ulong)dtt_header +
			be32_to_cpu(dtt_header->dt_entries_offset));

	entry_count = be32_to_cpu(dtt_header->dt_entry_count);

	for (i = 0; i < entry_count; i++) {
		printf("Entry %d - fname: %s\n", i, dtt_entry->fdt_fname);
		dtt_entry++;
	}
#endif
}

/**
 * Load into memory the fdt blob that matches the provided name.
 *
 * @param dt_fname file name of the devicetree to load
 * @param fdt_addr memory address where the dt will be loaded
 * @param dtt_header pointer to the dtbo partition header in memory
 *
 * @return the size in bytes of the device tree loaded
 */
u32 _load_fdt_by_name(char *dt_fname, ulong fdt_addr,
		      struct dt_table_header *dtt_header)
{
	struct dt_table_entry *dtt_entry;
	u32 i, entry_count, fdt_size = 0, fdt_offset;

	dtt_entry = (struct dt_table_entry *)((ulong)dtt_header +
			be32_to_cpu(dtt_header->dt_entries_offset));

	entry_count = be32_to_cpu(dtt_header->dt_entry_count);

	for (i = 0; i < entry_count; i++) {
		if (!(strncmp(dtt_entry->fdt_fname,
			      dt_fname, strlen(dt_fname)))) {
			fdt_size = be32_to_cpu(dtt_entry->dt_size);
			fdt_offset = be32_to_cpu(dtt_entry->dt_offset);

			memcpy((void *)fdt_addr,
			       (void *)((ulong)dtt_header + fdt_offset),
			       fdt_size);
			break;
		}
		dtt_entry++;
	}

	return fdt_size;
}

/**
 * Load into memory the fdt blob that matches the provided name.
 *
 * @param index index of the of the devicetree entry to load
 * @param fdt_addr memory address where the dt will be loaded
 * @param dtt_header pointer to the dtbo partition header in memory
 *
 * @return 0 on success, error code otherwise
 */
int load_fdt_by_index(int index, ulong fdt_addr,
		      struct dt_table_header *dtt_header)
{
	struct dt_table_entry *dtt_entry;
	u32 entry_count, fdt_size, fdt_offset;

	dtt_entry = (struct dt_table_entry *)((ulong)dtt_header +
			be32_to_cpu(dtt_header->dt_entries_offset));

	entry_count = be32_to_cpu(dtt_header->dt_entry_count);

	if (index >= entry_count)
		return -EINVAL;

	dtt_entry += index;
	fdt_size = be32_to_cpu(dtt_entry->dt_size);
	fdt_offset = be32_to_cpu(dtt_entry->dt_offset);

	memcpy((void *)fdt_addr,
	       (void *)((ulong) dtt_header + fdt_offset), fdt_size);

	return 0;
}

/**
 * Wrapper arround _load_fdt_by_name to return 0 on success or the error code
 * on failure
 */
int load_fdt_by_name(char *dt_fname, ulong fdt_addr,
		     struct dt_table_header *dtt_header)
{
	if (!_load_fdt_by_name(dt_fname, fdt_addr, dtt_header))
		return -EINVAL;

	return 0;
}

/**
 * Load and apply the device tree overlays included in the provided variable
 *
 * @param overlays_var environment variable containing the DT overlay list
 * @param fdt_addr memory address of the DT to apply the overlays
 * @param dtt_header pointer to the dtbo partition header in memory
 *
 * @return 0 on success, error code otherwise
 */
int apply_fdt_overlays(char *overlays_var, ulong fdt_addr,
		       struct dt_table_header *dtt_header)
{
#ifdef CONFIG_OF_LIBFDT_OVERLAY
	char cmd_buf[CONFIG_SYS_CBSIZE];
	char *env_overlay_list = NULL;
	char *overlay_list_copy = NULL;
	char *overlay_list = NULL;
	char *overlay = NULL;
	char *overlay_desc = NULL;
	int root_node;
	ulong loadaddr =
	    env_get_ulong("initrd_addr", 16, CONFIG_DIGI_UPDATE_ADDR);
	u32 fdt_size;

	sprintf(cmd_buf, "fdt addr %lx", fdt_addr);
	if (run_command(cmd_buf, 0)) {
		printf("Failed to set base fdt address to %lx\n", fdt_addr);
		return -EINVAL;
	}
	/* get the right fdt_blob from the global working_fdt */
	gd->fdt_blob = working_fdt;
	root_node = fdt_path_offset(gd->fdt_blob, "/");

	/* Copy the variable to avoid modifying it in memory */
	env_overlay_list = env_get(overlays_var);
	if (env_overlay_list) {
		printf("\n## Applying device tree overlays in variable '%s':\n", overlays_var);
		overlay_list_copy = strdup(env_overlay_list);
		if (overlay_list_copy) {
			overlay_list = overlay_list_copy;
			overlay = strtok(overlay_list, DELIM_OV_FILE);
		} else {
			printf("\n## Not enough memory to duplicate '%s'\n", overlays_var);
			return -ENOMEM;
		}
	} else {
		printf("\n## No device tree overlays present in variable '%s'\n", overlays_var);
	}

	while (overlay != NULL) {
		/* Load the overlay */
		fdt_size = _load_fdt_by_name(overlay, loadaddr, dtt_header);
		if (!fdt_size) {
			printf("Error loading overlay %s\n", overlay);
			free(overlay_list_copy);
			return -EINVAL;
		}

		/* Resize the base fdt to make room for the overlay */
		sprintf(cmd_buf, "fdt resize %x", fdt_size);
		if (run_command(cmd_buf, 0)) {
			printf("Error failed to resize fdt\n");
			free(overlay_list_copy);
			return -EINVAL;
		}

		/* Apply the overlay */
		sprintf(cmd_buf, "fdt apply %lx", loadaddr);
		if (run_command(cmd_buf, 0)) {
			printf("Error failed to apply overlay %s\n", overlay);
			free(overlay_list_copy);
			return -EINVAL;
		}

		/* Search for an overlay description */
		overlay_desc = (char *)fdt_getprop(gd->fdt_blob, root_node,
						   "overlay-description", NULL);

		/* Print the overlay filename (and description if available) */
		printf("-> %-50s", overlay);
		if (overlay_desc) {
			printf("%s", overlay_desc);
			/* remove property and reset pointer after printing */
			fdt_delprop((void *)gd->fdt_blob, root_node,
				    "overlay-description");
			overlay_desc = NULL;
		}

		overlay = strtok(NULL, DELIM_OV_FILE);
		if (overlay)
			printf("\n");
	}
	printf("\n");

	free(overlay_list_copy);

	return 0;
#else
	printf("Error overlay support not supported in this build\n");

	return -ENOSUP;
#endif /* CONFIG_OF_LIBFDT_OVERLAY */
}

/**
 * Load the devicetree and the overlays from the content of environment
 * varialbes
 */
int connectcore_load_fdt(ulong fdt_addr, struct dt_table_header *dtt_header)
{
#define MAIN_DTB_IDX	0
	char *fdt_file;
	int ret;
	char *som_overlays_override;

	dump_fdt_part_info(dtt_header);

	fdt_file = env_get("fdt_file");
	if (fdt_file) {
		ret = load_fdt_by_name(fdt_file, fdt_addr, dtt_header);
		if (ret) {
			printf("Error loading devicetree file %s\n", fdt_file);
			return -EINVAL;
		}
	} else {
		ret = load_fdt_by_index(MAIN_DTB_IDX, fdt_addr, dtt_header);
		if (ret) {
			printf("Error loading devicetree with index %d\n",
			       MAIN_DTB_IDX);
			return -EINVAL;
		}
	}

	som_overlays_override = env_get("som_overlays_override");
	ret = apply_fdt_overlays(som_overlays_override ?
				 "som_overlays_override" : "som_overlays",
				 fdt_addr, dtt_header);
	if (ret) {
		printf("Error loading 'som_overlays' var overlays\n");
		return -EINVAL;
	}

	ret = apply_fdt_overlays("overlays", fdt_addr, dtt_header);
	if (ret) {
		printf("Error loading 'overlays' var overlays\n");
		return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_ANDROID_LOAD_CONNECTCORE_FDT */

#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
int read_squashfs_rootfs(unsigned long addr, unsigned long *size)
{
	char cmd_buf[CONFIG_SYS_CBSIZE];
	unsigned long squashfs_size = 0, squashfs_raw_size = 0, squashfs_temp_addr = 0;
	uint32_t *squashfs_size_addr = NULL;
	uint32_t *squashfs_magic = NULL;
#ifdef CONFIG_AHAB_BOOT
	unsigned long squashfs_ahab_addr = 0;
#endif

#ifdef CONFIG_AHAB_BOOT
	/* We have placed signature container at the end of the image
	 * Now we need to put on top of the image again for
	 * authentication.
	 */
	squashfs_temp_addr = addr + AHAB_CONTAINER_SIZE;
#else
	squashfs_temp_addr = addr;
#endif

#ifdef CONFIG_NAND_BOOT
	int ret = 0;

	/* Access ubi partition */
	if (of_machine_is_compatible("digi,ccimx6ul")
		ret = activate_ubi_part(env_get_yesno("singlemtdsys") ?
					SYSTEM_PARTITION : ROOTFS_PARTITION);
	else
		ret = activate_ubi_part(SYSTEM_PARTITION);

	if (ret) {
		debug("Error: cannot find root partition or ubi volume\n");
		return -1;
	}

	/* Read squashfs header into RAM */
	sprintf(cmd_buf, "ubi read %lx ${rootfsvol} 100", squashfs_temp_addr);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to read from ubi partition\n");
		return -1;
	}
#else
	uint32_t blk_count;
	char rootfspart[32];

	if (env_get_yesno("dualboot")) {
		strcpy(rootfspart,
		       strcmp(env_get("active_system"), "linux_a") ?
		       "rootfs_b" : "rootfs_a");
	} else {
		strcpy(rootfspart, "rootfs");
	}

	sprintf(cmd_buf, "part start mmc ${mmcbootdev} %s rootfs_start",
		rootfspart);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to get start offset of %s partition\n",
		      rootfspart);
		return -1;
	}

	/* read first 32 sectors of rootfs image into RAM */
	sprintf(cmd_buf, "mmc read %lx ${rootfs_start} 20", squashfs_temp_addr);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to read block from mmc\n");
		return -1;
	}

#endif /* CONFIG_NAND_BOOT */

	/* Check if this is a squashfs image */
	squashfs_magic = (uint32_t *)map_sysmem(squashfs_temp_addr, 0);
	if (*squashfs_magic != SQUASHFS_MAGIC) {
		debug("Error: Rootfs is not Squashfs, abort authentication\n");
		return -1;
	}

	squashfs_size_addr = (uint32_t *)map_sysmem(squashfs_temp_addr + SQUASHFS_BYTES_USED_OFFSET, 0);
	if (squashfs_size_addr == NULL) {
		debug("Error: Failed to allocate \n");
		return -1;
	}
	squashfs_raw_size = *squashfs_size_addr;
	/* align address to next 4K value */
	squashfs_raw_size += 0xFFF;
	squashfs_raw_size &= 0xFFFFF000;

#ifdef CONFIG_AHAB_BOOT
	/* add signature size */
	squashfs_size = squashfs_raw_size + AHAB_CONTAINER_SIZE;
#elif defined(CONFIG_IMX_HAB)
	/* add signature size */
	squashfs_size = squashfs_raw_size + CONFIG_CSF_SIZE + IVT_SIZE;
#else
	/* TODO: add signature size */
	squashfs_size = squashfs_raw_size;
#endif

#ifdef CONFIG_NAND_BOOT
	sprintf(cmd_buf, "ubi read %lx ${rootfsvol} %lx", addr, squashfs_size);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to read squashfs image into RAM\n");
		return -1;
	}
#else
	blk_count = (squashfs_size / 0x200) + 1;
	sprintf(cmd_buf, "mmc read %lx ${rootfs_start} %x", squashfs_temp_addr, blk_count);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to read squashfs image into RAM\n");
		return -1;
	}
#ifdef CONFIG_AHAB_BOOT
	/* Now copy the signature container to the start */
	squashfs_ahab_addr = squashfs_temp_addr + squashfs_raw_size;
	memcpy((void *)addr,
		   (void *)squashfs_ahab_addr,
		   AHAB_CONTAINER_SIZE);
#endif
#endif /* CONFIG_NAND_BOOT */

	*size = squashfs_raw_size;

	return 0;
}
#endif /* CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS */

#ifdef CONFIG_ANDROID_SUPPORT
bool is_power_key_pressed(void) {
	unsigned char power_key_pressed;

	mca_read_reg(MCA_PWR_STATUS_0, &power_key_pressed);

	return (bool)power_key_pressed;
}
#endif

/*
 * Checks if the partition is a sensitive one and asks for confirmation if so.
 * Returns true if we can proceed with the update.
 */
bool proceed_with_update(char *name)
{
	char sensitive[256];
	char *part;
	char msg[256];

	strcpy(sensitive, SENSITIVE_PARTITIONS);
	part = strtok(sensitive, ",");
	while(part) {
		if (!strcmp(name, part)) {
			sprintf(msg,
				"Do you really want to program '%s' partition? <y/N> ",
				name);
			return confirm_msg(msg);
		}
		part = strtok(NULL, ",");
	}

	return true;
}
