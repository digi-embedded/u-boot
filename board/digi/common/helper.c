/*
 *  Copyright (C) 2017-2021 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <common.h>
#include <console.h>
#include <linux/errno.h>
#include <fsl_sec.h>
#include <asm/imx-common/hab.h>
#include <malloc.h>
#include <mapmem.h>
#include <nand.h>
#include <version.h>
#include <watchdog.h>
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif
#include <otf_update.h>
#include "helper.h"
#include "hwid.h"
DECLARE_GLOBAL_DATA_PTR;
#if defined(CONFIG_CMD_UPDATE_MMC) || defined(CONFIG_CMD_UPDATE_NAND)
#define CONFIG_CMD_UPDATE
#endif

#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
#define IVT_HEADER_SIZE				0x20
#define SQUASHFS_BYTES_USED_OFFSET	0x28
#define SQUASHFS_MAGIC				0x73717368
/*
 * If CONFIG_CSF_SIZE is undefined, assume 0x4000. This value will be used
 * in the signing script.
 */
#ifndef CONFIG_CSF_SIZE
#define CONFIG_CSF_SIZE 0x4000
#endif
#endif

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
				 disk_partition_t *partition)
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
	filesize = getenv_ulong("filesize", 16, 0);
	remaining = filesize;

	/* Init otf data */
	otfd.loadaddr = (void *)getenv_ulong("update_addr", 16, CONFIG_DIGI_UPDATE_ADDR );

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

#if defined(CONFIG_CMD_UPDATE) || defined(CONFIG_CMD_DBOOT)
bool is_image_compressed(void)
{
	char *var;

	var = getenv("dboot_kernel_var");
	if (var && !strcmp(var, "imagegz"))
		return true;

	return false;
}

int get_source(int argc, char * const argv[], struct load_fw *fwinfo)
{
	int i;
	char *src;
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
		if (argc > 3)
			fwinfo->devpartno = (char *)argv[3];
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
			printf("Cannot find '%s' partition\n", partname);
			goto _err;
		}
#endif
		break;
	}

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
			fwinfo->filename = argv[3];
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
			fwinfo->filename = argv[filename_index];
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
			var = getenv("dboot_kernel_var");
			if (var)
				return getenv(var);
			else
				return "$zimage";
		}
		break;

	case CMD_UPDATE:
		if (!strcmp(partname, "uboot")) {
			return "$uboot_file";
		} else {
			/* Read the default filename from a variable called
			 * after the partition name: <partname>_file
			 */
			sprintf(varname, "%s_file", partname);
			return getenv(varname);
		}
		break;
	}

	return NULL;
}

int get_default_devpartno(int src, char *devpartno)
{
	char *dev, *part;

	switch (src) {
	case SRC_MMC:
		dev = getenv("mmcdev");
		if (dev == NULL)
			return -1;
		part = getenv("mmcpart");
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

#ifdef CONFIG_DIGI_UBI
bool is_ubi_partition(struct part_info *part)
{
	struct mtd_info *nand = nand_info[0];
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
			filesize = getenv_ulong("filesize", 16, 0);

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
int load_firmware(struct load_fw *fwinfo)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	char def_devpartno[] = "0:1";
	int ret;
	int fwload = FWLOAD_YES;
	ulong loadaddr;

	/* 'fwinfo->varload' determines if the file must be loaded:
	 * - yes|NULL: the file must be loaded. Return error otherwise.
	 * - try: the file may be loaded. Return ok even if load fails.
	 * - no: skip the load.
	 */
	if (NULL != fwinfo->varload) {
		if (!strcmp(fwinfo->varload, "no"))
			return LDFW_NOT_LOADED;	/* skip load and return ok */
		else if (!strcmp(fwinfo->varload, "try"))
			fwload = FWLOAD_TRY;
	}

	/* Use default values if not provided */
	if (NULL == fwinfo->devpartno) {
		if (get_default_devpartno(fwinfo->src, def_devpartno))
			strcpy(def_devpartno, "0:1");
		fwinfo->devpartno = def_devpartno;
	}

	/*
	 * Load firmware to fwinfo->loadaddr except if loading a compressed
	 * image. Skip the leading dollar sign in the strings to obtain the
	 * value correctly
	 */
	loadaddr = fwinfo->compressed ?
	           getenv_ulong(fwinfo->lzipaddr + 1, 16, CONFIG_DIGI_LZIPADDR) :
	           getenv_ulong(fwinfo->loadaddr + 1, 16, CONFIG_DIGI_UPDATE_ADDR);

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
		if (is_ubi_partition(fwinfo->part)) {
			sprintf(cmd,
				"if ubi part %s;then "
					"if ubifsmount ubi0:%s;then "
						"ubifsload 0x%lx %s;"
#ifndef CONFIG_MTD_UBI_SKIP_REATTACH
						"ubifsumount;"
#endif
					"fi;"
				"fi;",
				fwinfo->part->name, fwinfo->part->name,
				loadaddr, fwinfo->filename);
		} else
#endif
		{
			sprintf(cmd, "nand read %s 0x%lx %x", fwinfo->part->name,
				loadaddr, (u32)fwinfo->part->size);
		}
		break;
	case SRC_RAM:
		ret = LDFW_NOT_LOADED;	/* file is already in RAM */
		goto _ret;
	default:
		return -1;
	}

	ret = run_command(cmd, 0);
	if (!ret && fwinfo->compressed) {
		ulong len, src, dest;

		src = getenv_ulong(fwinfo->lzipaddr + 1, 16, 0);
		dest = getenv_ulong(fwinfo->loadaddr + 1, 16, 0);
		len = getenv_ulong("filesize", 16, 0);

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

#if defined(CONFIG_SOURCE) && defined(CONFIG_AUTO_BOOTSCRIPT)
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
	char *retrycnt = getenv("netretry");

	bootscript = getenv("bootscript");
	if (bootscript) {
		printf("Bootscript from TFTP... ");

		/* Silence console */
		gd->flags |= GD_FLG_SILENT;
		/* set timeouts for bootscript */
		tftp_timeout_ms = AUTOSCRIPT_TFTP_MSEC;
		tftp_timeout_count_max = AUTOSCRIPT_TFTP_CNT;
		net_start_again_timeout = AUTOSCRIPT_START_AGAIN;
		/* set retrycnt */
		setenv("netretry", "no");

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
			setenv("netretry", retrycnt);
		else
			setenv("netretry", "");
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

	if (getc() == 'y') {
		int c;

		putc('y');
		c = getc();
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

	if ((tmp = getenv(varname)) != NULL) {
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
	char *regdomain = getenv("regdomain");

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

#define STR_HEX_CHUNK			8
/*
 * Convert string with hexadecimal characters into a hex number
 * @in: Pointer to input string
 * @out Pointer to output number array
 * @len Number of elements in the output array
*/
void strtohex(char *in, unsigned long *out, int len)
{
	char tmp[] = "ffffffff";
	int i, j;

	for (i = 0, j = 0; j < len; i += STR_HEX_CHUNK, j++) {
		strncpy(tmp, &in[i], STR_HEX_CHUNK);
		out[j] = cpu_to_be32(simple_strtol(tmp, NULL, 16));
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

	mac = getenv(var);
	if (NULL == mac)
		printf("   WARNING: MAC not set in '%s'\n", var);
	else if (!strcmp(mac, default_mac))
		printf("   WARNING: Dummy default MAC in '%s'\n", var);
}

/*
 * Check if the storage media address/block is empty
 * @in: Address/offset in media
 * @in: Partition index, only applies for MMC
 * The function returns:
 *	1 if the block is empty
 *	0 if the block is not empty
 *	-1 on error
 */
int media_block_is_empty(uintptr_t addr, uint hwpart)
{
	size_t len;
	int ret = -1;
	int i;
	uint64_t empty_pattern = 0;
	uint64_t *readbuf = NULL;

	if (strcmp(CONFIG_SYS_STORAGE_MEDIA, "nand") == 0)
		empty_pattern = ~0;

	len = media_get_block_size();
	if (!len)
		return ret;

	readbuf = malloc(len);
	if (!readbuf)
		return ret;

	if (media_read_block(addr, (unsigned char *)readbuf, hwpart))
		goto out_free;

	ret = 1;	/* media block empty */
	for (i = 0; i < len / 8; i++) {
		if (readbuf[i] != empty_pattern) {
			ret = 0;	/* media block not empty */
			break;
		}
	}
out_free:
	free(readbuf);
	return ret;
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

	 verifyaddr = loadaddr + ((gd->ram_size - (loadaddr - PHYS_SDRAM)) / 2);

	 /* Skip reserved memory area */
#if defined(RESERVED_MEM_START) && defined(RESERVED_MEM_END)
	if (verifyaddr >= RESERVED_MEM_START && verifyaddr < RESERVED_MEM_END)
		verifyaddr = RESERVED_MEM_END;
#endif

	 if (verifyaddr > loadaddr &&
	     verifyaddr < (PHYS_SDRAM + gd->ram_size))
		 setenv_hex("verifyaddr", verifyaddr);
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
#ifdef CONFIG_RNG_SW_TEST
	uint8_t *res_ptr;
	uint32_t res_addr = getenv_ulong("loadaddr", 16, CONFIG_LOADADDR);
#endif

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
#ifdef CONFIG_RNG_SW_TEST
		printf("RNG:   self-test failure detected, will run software self-test\n");
		res_ptr = map_sysmem(res_addr, 32);
		ret = rng_sw_test(res_ptr);

		if (ret == 0)
			ret = SW_RNG_TEST_PASSED;
		else
#endif
			ret = SW_RNG_TEST_FAILED;
	}

	return ret;
}
#endif /* CONFIG_HAS_TRUSTFENCE */

#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
int read_squashfs_rootfs(unsigned long addr, unsigned long *size) {
	char cmd_buf[CONFIG_SYS_CBSIZE];
	unsigned long squashfs_size = 0, squashfs_raw_size = 0;
	uint32_t *squashfs_size_addr = NULL;
	uint32_t *squashfs_magic = NULL;
	uint32_t blk_count;
	char rootfspart[32];

	if (strcmp(getenv("dualboot"), "yes") == 0) {
		strcpy(rootfspart,
		       strcmp(getenv("active_system"), "linux_a") ?
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

	/* read squashfs header into RAM */
	sprintf(cmd_buf, "mmc read %lx ${rootfs_start} 1", addr);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to read block from mmc\n");
		return -1;
	}

	/* Check if this is a squashfs image */
	squashfs_magic = (uint32_t *)map_sysmem(addr, 0);
	if (*squashfs_magic != SQUASHFS_MAGIC) {
		printf("Error: Rootfs is not Squashfs, abort authentication\n");
		return -1;
	}

	squashfs_size_addr = (uint32_t *)map_sysmem(addr + SQUASHFS_BYTES_USED_OFFSET, 0);
	if (squashfs_size_addr == NULL) {
		debug("Error: Failed to allocate \n");
		return -1;
	}
	squashfs_raw_size = *squashfs_size_addr;
	/* align address to next 4K value */
	squashfs_raw_size += 0xFFF;
	squashfs_raw_size &= 0xFFFFF000;
	/* add signature size */
	squashfs_size = squashfs_raw_size + CONFIG_CSF_SIZE + IVT_HEADER_SIZE;

	blk_count = (squashfs_size / 0x200) + 1;
	sprintf(cmd_buf, "mmc read %lx ${rootfs_start} %x", addr, blk_count);
	if (run_command(cmd_buf, 0)) {
		debug("Failed to read squashfs image into RAM\n");
		return -1;
	}

	*size = squashfs_raw_size;

	return 0;
}
#endif /* CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS */
