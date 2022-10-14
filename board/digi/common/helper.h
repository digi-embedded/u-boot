/*
 *  Copyright (C) 2014-2020 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __DIGI_HELPER_H
#define __DIGI_HELPER_H

#include <blk.h>
#include <jffs2/load_kernel.h>

enum {
	SRC_UNDEFINED = -2,
	SRC_UNSUPPORTED = -1,
	SRC_TFTP,
	SRC_NFS,
	SRC_NAND,
	SRC_USB,
	SRC_MMC,
	SRC_RAM,
	SRC_SATA,
};

enum {
	CMD_DBOOT,
	CMD_UPDATE,
};

enum {
	LDFW_ERROR = -1,
	LDFW_NOT_LOADED,
	LDFW_LOADED,
};

struct load_fw {
	bool compressed;
	int src;
	char filename[256];
	char devpartno[10];
	char loadaddr[20];
	char lzipaddr[20];
	char varload[20];
	struct part_info *part;
	bool ubivol;
	char ubivolname[30];
};

#define SW_RNG_TEST_FAILED 	1
#define SW_RNG_TEST_PASSED 	2
#define SW_RNG_TEST_NA 		3

#define UBIFS_MAGIC		0x06101831
#define SQUASHFS_MAGIC		0x73717368

int confirm_msg(char *msg);
int get_source(int argc, char * const argv[], struct load_fw *fwinfo);
bool is_image_compressed(void);
const char *get_source_string(int src);
int get_fw_filename(int argc, char * const argv[], struct load_fw *fwinfo);
char *get_default_filename(char *partname, int cmd);
#ifdef CONFIG_DIGI_UBI
bool is_ubi_partition(struct part_info *part);
#endif
int strtou32(const char *str, unsigned int base, u32 *result);
int confirm_prog(void);
void fdt_fixup_mac(void *fdt, char *varname, char *node, char *property);
void fdt_fixup_regulatory(void *fdt);
void fdt_fixup_uboot_info(void *fdt);
unsigned long get_firmware_size(const struct load_fw *fwinfo);
int load_firmware(struct load_fw *fwinfo, char *msg);
const char *get_filename_ext(const char *filename);
void strtohex(char *in, unsigned char *out, int len);
void verify_mac_address(char *var, char *default_mac);
int media_read_block(uintptr_t addr, unsigned char *readbuf, uint hwpart);
int media_write_block(uintptr_t addr, unsigned char *readbuf, uint hwpart);
size_t media_get_block_size(void);
u64 memsize_parse(const char *const ptr, const char **retptr);
void set_verifyaddr(unsigned long loadaddr);
bool validate_bootloader_image(void *loadaddr);
int hab_event_warning_check(uint8_t *event, size_t *bytes);
#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
int read_squashfs_rootfs(unsigned long addr, unsigned long *size);
#endif
ulong bootloader_mmc_offset(void);
#ifdef CONFIG_ANDROID_SUPPORT
bool is_power_key_pressed(void);
#endif
bool proceed_with_update(char *name);
#endif  /* __DIGI_HELPER_H */
