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

enum {
	OS_UNDEFINED = -1,
	OS_LINUX,
	OS_ANDROID,
};

static const char *os_strings[] = {
	[OS_LINUX] =	"linux",
	[OS_ANDROID] =	"android",
};

static int get_os(char *os)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(os_strings); i++) {
		if (!strncmp(os_strings[i], os, strlen(os)))
			return i;
	}

	return OS_UNDEFINED;
}

static const char *get_os_string(int os)
{
	if (OS_UNDEFINED != os && os < ARRAY_SIZE(os_strings))
		return os_strings[os];

	return "";
}

static int set_bootargs(int os, int src)
{
	char var[100] = "";
	char cmd[CONFIG_SYS_CBSIZE] = "";

	/*
	 * If using 'dboot' in recovery mode, run the script at variable
	 * $bootargs_recovery.
	 */
	if (env_get_yesno("boot_recovery") == 1)
		sprintf(var, "bootargs_recovery");

	/*
	 * If using 'dboot' in normal mode, or $bootargs_recovery does not
	 * exist, run the script in $bootargs_<src>_<os>.
	 */
	if (!env_get(var))
		sprintf(var, "bootargs_%s_%s", get_source_string(src),
			get_os_string(os));

	sprintf(cmd, "run %s", var);

	return run_command(cmd, 0);
}

static int boot_os(int has_initrd, int has_fdt)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	char *var;
	char dboot_cmd[] = "bootz";	/* default */

	var = env_get("dboot_kernel_var");
	if (var) {
		if (!strcmp(var, "uimage"))
			strcpy(dboot_cmd, "bootm");
		else if (!strcmp(var, "image"))
			strcpy(dboot_cmd, "booti");
		else if (!strcmp(var, "imagegz"))
			strcpy(dboot_cmd, "booti");
	}

	sprintf(cmd, "%s $loadaddr %s %s", dboot_cmd,
		has_initrd ? "$initrd_addr" : "-",
		has_fdt ? "$fdt_addr" : "");

	return run_command(cmd, 0);
}

static int do_dboot(cmd_tbl_t* cmdtp, int flag, int argc, char * const argv[])
{
	int os = SRC_UNDEFINED;
	int ret;
	int has_fdt = 0;
	int has_initrd = 0;
	struct load_fw fwinfo;

	if (argc < 2)
		return CMD_RET_USAGE;

	memset(&fwinfo, 0, sizeof(fwinfo));

	/* Get OS to boot */
	os = get_os(argv[1]);
	if (OS_UNDEFINED == os) {
		printf("'%s' is not a valid operating system\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	/* Get source of firmware file */
	if (get_source(argc, argv, &fwinfo))
		return CMD_RET_FAILURE;

	/* Get firmware file name */
	ret = get_fw_filename(argc, argv, &fwinfo);
	if (ret) {
		/* Filename was not provided. Look for default one */
		fwinfo.filename = get_default_filename(argv[1], CMD_DBOOT);
		if (!fwinfo.filename) {
			printf("Error: need a filename\n");
			return CMD_RET_FAILURE;
		}
	}

	/* Load firmware file to RAM */
	fwinfo.compressed = is_image_compressed();
	fwinfo.loadaddr = "$loadaddr";
	fwinfo.lzipaddr = "$lzipaddr";

	ret = load_firmware(&fwinfo);
	if (ret == LDFW_ERROR) {
		printf("Error loading firmware file to RAM\n");
		return CMD_RET_FAILURE;
	}

	/* Get flattened Device Tree */
	fwinfo.varload = env_get("boot_fdt");
	if (NULL == fwinfo.varload)
		fwinfo.varload = "try";
	fwinfo.loadaddr = "$fdt_addr";
	fwinfo.filename = "$fdt_file";
	fwinfo.compressed = false;
	ret = load_firmware(&fwinfo);
	if (ret == LDFW_LOADED) {
		has_fdt = 1;
	} else if (ret == LDFW_ERROR) {
		printf("Error loading FDT file\n");
		return CMD_RET_FAILURE;
	}

	/* Get init ramdisk */
	fwinfo.varload = env_get("boot_initrd");
	if (NULL == fwinfo.varload && OS_LINUX == os)
		fwinfo.varload = "no";	/* Linux default */
	fwinfo.loadaddr = "$initrd_addr";
	fwinfo.filename = "$initrd_file";
	ret = load_firmware(&fwinfo);
	if (ret == LDFW_LOADED) {
		has_initrd = 1;
	} else if (ret == LDFW_ERROR) {
		printf("Error loading init ramdisk file\n");
		return CMD_RET_FAILURE;
	}

	/* Set boot arguments */
	ret = set_bootargs(os, fwinfo.src);
	if (ret) {
		printf("Error setting boot arguments\n");
		return CMD_RET_FAILURE;
	}

	/* Boot OS */
	return boot_os(has_initrd, has_fdt);
}

U_BOOT_CMD(
	dboot,	6,	0,	do_dboot,
	"Digi modules boot command",
	"<os> [source] [extra-args...]\n"
	" Description: Boots <os> via <source>\n"
	" Arguments:\n"
	"   - os:           one of the operating systems reserved names: \n"
	"                   linux|android\n"
	"   - [source]:     " CONFIG_DBOOT_SUPPORTED_SOURCES_LIST "\n"
	"   - [extra-args]: extra arguments depending on 'source'\n"
	"\n"
	CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP
);
