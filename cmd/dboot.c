/*
 *  Copyright (C) 2014-2021 by Digi International Inc.
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
#include <part.h>
#ifdef CONFIG_OF_LIBFDT_OVERLAY
#include <stdlib.h>
#endif
#include <linux/libfdt.h>
#include "../board/digi/common/helper.h"
#include <image.h>
#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
#include "../board/digi/common/auth.h"
#endif /* CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS */

DECLARE_GLOBAL_DATA_PTR;

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

static int boot_os(char* initrd_addr, char* fdt_addr)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	char *var;
	char dboot_cmd[] = "bootz";	/* default */

	/*
	 * Get kernel type looking at first char
	 *
	 * image    -> booti
	 * imagegz  -> booti
	 * uimage   -> bootm
	 * zimage   -> bootz
	 */
	var = env_get("dboot_kernel_var");
	switch (var[0]) {
	case 'u':
		strcpy(dboot_cmd, "bootm");
		break;
	case 'i':
		strcpy(dboot_cmd, "booti");
		break;
	}

	sprintf(cmd, "%s $loadaddr %s %s", dboot_cmd,
		(initrd_addr && !initrd_addr[0]) ? "-" : initrd_addr,
		(fdt_addr && !fdt_addr[0]) ? "" : fdt_addr);

	return run_command(cmd, 0);
}

static int do_dboot(struct cmd_tbl* cmdtp, int flag, int argc, char * const argv[])
{
	int os = SRC_UNDEFINED;
	int ret;
	char fdt_addr[20] = "";
	char initrd_addr[20] = "";
	char *var;
#ifdef CONFIG_OF_LIBFDT_OVERLAY
	char cmd_buf[CONFIG_SYS_CBSIZE];
	char *original_overlay_list;
	char *overlay_list = NULL;
	char *overlay = NULL;
	char *overlay_desc = NULL;
	int root_node;
#define DELIM_OV_FILE		","
#endif
	struct load_fw fwinfo;
#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
	unsigned long squashfs_raw_size;
	unsigned long rootfs_auth_addr;
#endif

	if (argc < 2)
		return CMD_RET_USAGE;

#ifdef CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS
	printf("## Loading squashfs: \n");
	/* Authenticate SQUASHFS rootfs */
#ifdef CONFIG_AHAB_BOOT
	rootfs_auth_addr = CONFIG_AUTH_SQUASHFS_ADDR;
#else
	rootfs_auth_addr = env_get_ulong("initrd_addr", 16, ~0UL);
	if (rootfs_auth_addr == ~0UL) {
		printf("Wrong initrd_addr. Can't read root file system\n");
		return CMD_RET_FAILURE;
	}
#endif

	if (read_squashfs_rootfs(rootfs_auth_addr, &squashfs_raw_size)) {
		printf("Error reading SQUASHFS root file system\n");
		return CMD_RET_FAILURE;
	}

	if (digi_auth_image(&rootfs_auth_addr, squashfs_raw_size) != 0){
		printf("Failed to authenticate rootfs image\n");
		return CMD_RET_FAILURE;
	}
#endif /* CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS */

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
		strncpy(fwinfo.filename,
			get_default_filename(argv[1], CMD_DBOOT),
			sizeof(fwinfo.filename));
		if (strlen(fwinfo.filename) == 0) {
			printf("Error: need a filename\n");
			return CMD_RET_FAILURE;
		}
	}

	/* Load firmware file to RAM */
	fwinfo.compressed = is_image_compressed();
	strncpy(fwinfo.loadaddr, "$loadaddr", sizeof(fwinfo.loadaddr));
	strncpy(fwinfo.lzipaddr, "$lzipaddr", sizeof(fwinfo.lzipaddr));

	ret = load_firmware(&fwinfo, "\n## Loading kernel");
	if (ret == LDFW_ERROR) {
		printf("Error loading firmware file to RAM\n");
		return CMD_RET_FAILURE;
	}

	/* Get flattened Device Tree */
	var = env_get("boot_fdt");
	if (var)
		strncpy(fwinfo.varload, var, sizeof(fwinfo.varload));
	else
		strcpy(fwinfo.varload, "");

	if (strlen(fwinfo.varload) == 0)
		strcpy(fwinfo.varload, "try");
	strncpy(fwinfo.loadaddr, "$fdt_addr", sizeof(fwinfo.loadaddr));
	strncpy(fwinfo.filename, "$fdt_file", sizeof(fwinfo.filename));
	fwinfo.compressed = false;
	ret = load_firmware(&fwinfo,
		"\n## Loading device tree file in variable 'fdt_file'");
	if (ret == LDFW_LOADED) {
		strcpy(fdt_addr, fwinfo.loadaddr);
	} else if (ret == LDFW_ERROR) {
		printf("Error loading FDT file\n");
		return CMD_RET_FAILURE;
	}

#ifdef CONFIG_OF_LIBFDT_OVERLAY
#ifdef CONFIG_AUTH_ARTIFACTS
	fdt_file_init_authentication();
	if (fdt_file_authenticate(fwinfo.loadaddr) != 0) {
		printf("Error authenticating FDT file\n");
		return CMD_RET_FAILURE;
	}
	/* Set FDT start address */
	strcpy(fdt_addr, fwinfo.loadaddr);
#endif /* CONFIG_AUTH_ARTIFACTS */
	sprintf(cmd_buf, "fdt addr %s", fwinfo.loadaddr);
	if (run_command(cmd_buf, 0)) {
		printf("Failed to set base fdt address\n");
		return CMD_RET_FAILURE;
	}
	/* get the right fdt_blob from the global working_fdt */
	gd->fdt_blob = working_fdt;
	root_node = fdt_path_offset(gd->fdt_blob, "/");

	/* Set firmware info common for all overlay files */
	strcpy(fwinfo.varload, "try");
	fwinfo.compressed = false;

	/* Copy the variable to avoid modifying it in memory */
	original_overlay_list = env_get("overlays");
	if (original_overlay_list)
		overlay_list = strdup(original_overlay_list);

	if (overlay_list) {
		overlay = strtok(overlay_list, DELIM_OV_FILE);
		printf("\n## Applying device tree overlays in variable 'overlays':\n");
	} else {
		printf("\n## No device tree overlays present in variable 'overlays'\n");
	}

	while (overlay != NULL) {
		strncpy(fwinfo.filename, overlay, sizeof(fwinfo.filename));
		strcpy(fwinfo.loadaddr, "$initrd_addr");
		ret = load_firmware(&fwinfo, NULL);

		if (ret != LDFW_LOADED) {
			printf("Error loading overlay %s\n", overlay);
			free(overlay_list);
			return CMD_RET_FAILURE;
		}

#ifdef CONFIG_AUTH_ARTIFACTS
		if (fdt_file_authenticate(fwinfo.loadaddr) != 0) {
			printf("Error authenticating FDT overlay file '%s'\n", fwinfo.filename);
			return CMD_RET_FAILURE;
		}
#endif /* CONFIG_AUTH_ARTIFACTS */

		/* Resize the base fdt to make room for the overlay */
		run_command("fdt resize $filesize", 0);

		sprintf(cmd_buf, "fdt apply %s", fwinfo.loadaddr);
		if (run_command(cmd_buf, 0)) {
			printf("Failed to apply overlay %s\n", overlay);
			free(overlay_list);
			return CMD_RET_FAILURE;
		}
		/* Search for an overlay description */
		overlay_desc = (char *)fdt_getprop(gd->fdt_blob, root_node,
						   "overlay-description", NULL);

		/* Print the overlay filename (and description if available) */
		printf("-> %-50s", overlay);
		if (overlay_desc) {
			printf("%s", overlay_desc);
			/* remove property and reset pointer after printing */
			fdt_delprop((void*)gd->fdt_blob, root_node,
				    "overlay-description");
			overlay_desc = NULL;
		}
		printf("\n");

		overlay = strtok(NULL, DELIM_OV_FILE);
	}
	printf("\n");

	if (overlay_list)
		free(overlay_list);
#endif /* CONFIG_OF_LIBFDT_OVERLAY */

	/* Get init ramdisk */
	var = env_get("boot_initrd");
	if (var)
		strncpy(fwinfo.varload, var, sizeof(fwinfo.varload));
	else
		strcpy(fwinfo.varload, "");

	if (strlen(fwinfo.varload) == 0 && OS_LINUX == os)
		strcpy(fwinfo.varload, "no");	/* Linux default */
	strcpy(fwinfo.loadaddr, "$initrd_addr");
	strcpy(fwinfo.filename, "$initrd_file");
	ret = load_firmware(&fwinfo, "\n## Loading init ramdisk");
	if (ret == LDFW_LOADED) {
		strcpy(initrd_addr, fwinfo.loadaddr);
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
	return boot_os(initrd_addr, fdt_addr);
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
