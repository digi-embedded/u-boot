/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __DIGI_HELPER_H
#define __DIGI_HELPER_H

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
	int src;
	char *filename;
	char *devpartno;
	char *fs;
	char *loadaddr;
	char *varload;
	struct part_info *part;
};

int confirm_msg(char *msg);
int get_source(int argc, char * const argv[], struct load_fw *fwinfo);
const char *get_source_string(int src);
int get_fw_filename(int argc, char * const argv[], struct load_fw *fwinfo);
char *get_default_filename(char *partname, int cmd);
#ifdef CONFIG_DIGI_UBI
bool is_ubi_partition(struct part_info *part);
#endif
int strtou32(const char *str, unsigned int base, u32 *result);
int confirm_prog(void);
void fdt_fixup_mac(void *fdt, char *varname, char *node);
#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
int console_enable_passphrase(void);
#endif

int load_firmware(struct load_fw *fwinfo);
const char *get_filename_ext(const char *filename);

#endif  /* __DIGI_HELPER_H */
