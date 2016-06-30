/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <common.h>
#include <asm/errno.h>
#include <malloc.h>
#include <nand.h>
#include <watchdog.h>
#include <u-boot/sha256.h>
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif
#include <otf_update.h>
#include "helper.h"

DECLARE_GLOBAL_DATA_PTR;
#if defined(CONFIG_CMD_UPDATE_MMC) || defined(CONFIG_CMD_UPDATE_NAND)
#define CONFIG_CMD_UPDATE
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

/* hook for on-the-fly update and register function */
static int (*otf_update_hook)(otf_data_t *data) = NULL;
/* Data struct for on-the-fly update */
static otf_data_t otfd;
#endif

#ifdef CONFIG_AUTO_BOOTSCRIPT
#define AUTOSCRIPT_TFTP_MSEC		100
#define AUTOSCRIPT_TFTP_CNT		15
#define AUTOSCRIPT_START_AGAIN		100
extern ulong TftpRRQTimeoutMSecs;
extern int TftpRRQTimeoutCountMax;
extern unsigned long NetStartAgainTimeout;
int DownloadingAutoScript = 0;
int RunningAutoScript = 0;
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

#if defined(CONFIG_CMD_UPDATE) || defined(CONFIG_CMD_DBOOT)
int get_source(int argc, char * const argv[], struct load_fw *fwinfo)
{
	int i;
	char *src;
	struct mtd_device *dev;
	u8 pnum;
	char *partname;

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

	switch (i) {
	case SRC_USB:
	case SRC_MMC:
	case SRC_SATA:
		/* Get device:partition and file system */
		if (argc > 3)
			fwinfo->devpartno = (char *)argv[3];
		if (argc > 4)
			fwinfo->fs = (char *)argv[4];
		break;
	case SRC_NAND:
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
		break;
	}

	fwinfo->src = i;
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
		if (argc > 5) {
			fwinfo->filename = argv[5];
			return 0;
		}
		break;
	case SRC_NAND:
		if (argc > 4) {
			fwinfo->filename = argv[4];
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
	switch(cmd) {
	case CMD_DBOOT:
		if (!strcmp(partname, "linux") ||
		    !strcmp(partname, "android")) {
			return "$uimage";
		}
		break;

	case CMD_UPDATE:
		if (!strcmp(partname, "uboot")) {
			return "$uboot_file";
		} else {
			/* Read the default filename from a variable called
			 * after the partition name: <partname>_file
			 */
			char varname[100];

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
	struct mtd_info *nand = &nand_info[0];
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
#endif /* CONFIG_CMD_UPDATE || CONFIG_CMD_DBOOT */

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
	otfd.loadaddr = getenv_ulong("loadaddr", 16, 0);

	while (remaining > 0) {
		debug("%lu remaining bytes\n", remaining);
		/* Determine chunk length to write */
		if (remaining > CONFIG_OTF_CHUNK) {
			otfd.len = CONFIG_OTF_CHUNK;
		} else {
			otfd.flags |= OTF_FLAG_FLUSH;
			otfd.len = remaining;
		}

		/* Load 'len' bytes from file[offset] into RAM */
		sprintf(cmd, "load %s %s $loadaddr %s %x %x", src_strings[src],
			devpartno, filename, otfd.len, (unsigned int)offset);
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

	switch (fwinfo->src) {
	case SRC_TFTP:
		sprintf(cmd, "tftpboot %s %s", fwinfo->loadaddr,
			fwinfo->filename);
		break;
	case SRC_NFS:
		sprintf(cmd, "nfs %s $rootpath/%s", fwinfo->loadaddr,
			fwinfo->filename);
		break;
	case SRC_MMC:
	case SRC_USB:
	case SRC_SATA:
		if (otf_update_hook) {
			ret = write_file_fs_otf(fwinfo->src, fwinfo->filename,
						fwinfo->devpartno);
			goto _ret;
		} else {
			sprintf(cmd, "load %s %s %s %s", src_strings[fwinfo->src],
				fwinfo->devpartno, fwinfo->loadaddr,
				fwinfo->filename);
		}
		break;
	case SRC_NAND:
		/*
		 * If the partition is UBI formatted, use 'ubiload' to read
		 * a file from the UBIFS file system. Otherwise use a raw
		 * read using 'nand read'.
		 */
		if (is_ubi_partition(fwinfo->part)) {
			sprintf(cmd, "ubi part %s;ubifsmount ubi0:%s;ubifsload %s %s",
				fwinfo->part->name, fwinfo->part->name,
				fwinfo->loadaddr, fwinfo->filename);
		} else {
			sprintf(cmd, "nand read %s %s %x", fwinfo->part->name,
				fwinfo->loadaddr, (u32)fwinfo->part->size);
		}
		break;
	case SRC_RAM:
		ret = LDFW_NOT_LOADED;	/* file is already in RAM */
		goto _ret;
	default:
		return -1;
	}

	ret = run_command(cmd, 0);
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
#endif /* CONFIG_CMD_UPDATE */

#if defined(CONFIG_SOURCE) && defined(CONFIG_AUTO_BOOTSCRIPT)
void run_auto_bootscript(void)
{
#ifdef CONFIG_CMD_NET
	int ret;
	char *bootscript;
	/* Save original timeouts */
        ulong saved_rrqtimeout_msecs = TftpRRQTimeoutMSecs;
        int saved_rrqtimeout_count = TftpRRQTimeoutCountMax;
	ulong saved_startagain_timeout = NetStartAgainTimeout;
	unsigned long saved_flags = gd->flags;
	char *retrycnt = getenv("netretry");

	bootscript = getenv("bootscript");
	if (bootscript) {
		printf("Bootscript from TFTP... ");

		/* Silence console */
		gd->flags |= GD_FLG_SILENT;
		/* set timeouts for bootscript */
		TftpRRQTimeoutMSecs = AUTOSCRIPT_TFTP_MSEC;
		TftpRRQTimeoutCountMax = AUTOSCRIPT_TFTP_CNT;
		NetStartAgainTimeout = AUTOSCRIPT_START_AGAIN;
		/* set retrycnt */
		setenv("netretry", "no");

		/* Silence net commands during the bootscript download */
		DownloadingAutoScript = 1;
		ret = run_command("tftp ${loadaddr} ${bootscript}", 0);
		/* First restore original values of global variables
		 * and then evaluate the result of the run_command */
		DownloadingAutoScript = 0;
		/* Restore original timeouts */
		TftpRRQTimeoutMSecs = saved_rrqtimeout_msecs;
		TftpRRQTimeoutCountMax = saved_rrqtimeout_count;
		NetStartAgainTimeout = saved_startagain_timeout;
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
void fdt_fixup_mac(void *fdt, char *varname, char *node)
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
		do_fixup_by_path(fdt, node, "mac-address", &mac_addr, 6, 1);
	}
}
#endif /* CONFIG_OF_BOARD_SETUP */

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

#if defined(CONFIG_CONSOLE_ENABLE_PASSPHRASE) || defined(CONFIG_ENV_AES_KEY)
#define STR_HEX_CHUNK 			8

/*
 * Convert string with hexadecimal characters into a hex number
 * @in: Pointer to input string
 * @out Pointer to output number array
 * @len Number of elements in the output array
*/
static void strtohex(char * in, unsigned long * out, int len)
{
	char tmp[] = "ffffffff";
	int i, j;

	for(i = 0, j = 0; j < len; i += STR_HEX_CHUNK, j++) {
		strncpy(tmp, &in[i], STR_HEX_CHUNK);
		out[j] = cpu_to_be32(simple_strtol(tmp, NULL, 16));
	}
}
#endif

#if defined(CONFIG_CONSOLE_ENABLE_PASSPHRASE)
#define INACTIVITY_TIMEOUT 		2

/*
 * Grab a passphrase from the console input
 * @secs: Inactivity timeout in seconds
 * @buff: Pointer to passphrase output buffer
 * @len: Length of output buffer
 *
 * Returns zero on success and a negative number on error.
 */
static int console_get_passphrase(int secs, char *buff, int len)
{
	char c;
	uint64_t end_tick, timeout;
	int i;

	/* Set a global timeout to avoid DoS attacks */
	end_tick = get_ticks() + (uint64_t)(secs * get_tbclk());

	/* Set an inactivity timeout */
	timeout = get_ticks() + INACTIVITY_TIMEOUT * get_tbclk();

	*buff = '\0';
	for (i = 0; i < len;) {
		/* Check timeouts */
		uint64_t tks = get_ticks();

		if ((tks > end_tick) || (tks > timeout)) {
			*buff = '\0';
			return -ETIME;
		}

		/* Do not trigger watchdog while typing passphrase */
		WATCHDOG_RESET();

		if (tstc()) {
			c = getc();
			i++;
		} else
			continue;

		switch (c) {
			/* Enter */
			case '\r':
			case '\n':
				*buff = '\0';
				return 0;
			/* nul */
			case '\0':
				continue;
			/* Ctrl-c */
			case 0x03:
				*buff = '\0';
				return -EINVAL;
			default:
				*buff++ = c;
				/* Restart inactivity timeout */
				timeout = get_ticks() + INACTIVITY_TIMEOUT *
					get_tbclk();
		}
	}

	return -EINVAL;
}

#define SHA256_HASH_LEN 		32
#define PASSPHRASE_SECS_TIMEOUT	10
#define MAX_PP_LEN 			64

/*
 * Returns zero (success) to enable the console, or a non zero otherwise.
 *
 * A sha256 hash is 256 bits (32 bytes) long, and is represented as
 * a 64 digits hex number.
 */
int console_enable_passphrase(void)
{
	char * pp = NULL;
	unsigned char *sha256_pp = NULL;
	unsigned long *pp_hash = NULL;
	int ret = -EINVAL;

	pp = malloc(MAX_PP_LEN + 1);
	if (!pp) {
		debug("Not enough memory for passphrase\n");
		return -ENOMEM;
	}

	ret = console_get_passphrase(PASSPHRASE_SECS_TIMEOUT,
				     pp, MAX_PP_LEN);
	if (ret)
		goto pp_error;

	sha256_pp = malloc(SHA256_HASH_LEN);
	if (!sha256_pp) {
		debug("Not enough memory for passphrase\n");
		ret = -ENOMEM;
		goto pp_error;
	}

	sha256_csum_wd((const unsigned char *)pp, strlen(pp),
			sha256_pp, CHUNKSZ_SHA256);

	pp_hash = malloc(SHA256_HASH_LEN);
	if (!pp_hash) {
		debug("Not enough memory for passphrase\n");
		ret = -ENOMEM;
		goto pp_hash_error;
	}

	memset(pp_hash, 0, SHA256_HASH_LEN);
	strtohex(CONFIG_CONSOLE_ENABLE_PASSPHRASE_KEY, pp_hash,
			SHA256_HASH_LEN/sizeof(unsigned long));
	ret = memcmp(sha256_pp, pp_hash, SHA256_HASH_LEN);

	free(pp_hash);
pp_hash_error:
	free(sha256_pp);
pp_error:
	free(pp);
	return ret;
}
#endif

#ifdef CONFIG_ENV_AES_KEY
/*
 * CONFIG_ENV_AES_KEY is a 128 bits (16 bytes) AES key, represented as
 * 32 hexadecimal characters.
 */
unsigned long key[4];

uint8_t *env_aes_cbc_get_key(void)
{
	if (strlen(CONFIG_ENV_AES_KEY) != 32) {
		puts("[ERROR] Wrong CONFIG_ENV_AES_KEY size "
			"(should be 128 bits)\n");
		return NULL;
	}

	strtohex(CONFIG_ENV_AES_KEY, key,
		 sizeof(key) / sizeof(unsigned long));

	return (uint8_t *) key;
}
#endif
