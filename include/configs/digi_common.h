/*
 *  include/configs/digi_common.h
 *
 *  Copyright (C) 2006-2018 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
/*
 *  !Revision:   $Revision: 1.4 $:
 *  !Author:     Markus Pietrek
 *  !Descr:      Defines all definitions that are common to all DIGI platforms
*/

#ifndef __DIGI_COMMON_H
#define __DIGI_COMMON_H

#include <linux/sizes.h>

#define DIGI_PLATFORM

/*
 * If we are developing, we might want to start armboot from ram
 * so we MUST NOT initialize critical regs like mem-timing ...
 */
/* define for developing */
#undef CONFIG_SKIP_LOWLEVEL_INIT

#define CONFIG_AUTO_BOOTSCRIPT
#define CONFIG_BOOTSCRIPT		CONFIG_SYS_BOARD "-boot.scr"

#define DEFAULT_MAC_ETHADDR	"00:04:f3:ff:ff:fa"
#define DEFAULT_MAC_WLANADDR	"00:04:f3:ff:ff:fb"
#define DEFAULT_MAC_BTADDR	"00:04:f3:ff:ff:fc"
#define DEFAULT_MAC_ETHADDR1	"00:04:f3:ff:ff:fd"
#define CONFIG_NO_MAC_FROM_OTP

#undef CONFIG_ENV_OVERWRITE
#define CONFIG_OVERWRITE_ETHADDR_ONCE
#define CONFIG_DEFAULT_NETWORK_SETTINGS	\
	"ethaddr=" DEFAULT_MAC_ETHADDR "\0" \
	"wlanaddr=" DEFAULT_MAC_WLANADDR "\0" \
	"btaddr=" DEFAULT_MAC_BTADDR "\0" \
	"ipaddr=192.168.42.30\0" \
	"serverip=192.168.42.1\0" \
	"netmask=255.255.0.0\0"

#define CONFIG_ROOTPATH		"/exports/nfsroot-" CONFIG_SYS_BOARD

#define CARRIERBOARD_VERSION_UNDEFINED	0
#define CARRIERBOARD_ID_UNDEFINED	0

/* ********** misc stuff ********** */
#define CONFIG_CMD_ENV_FLAGS
#undef CONFIG_SYS_MAXARGS
#define CONFIG_SYS_MAXARGS	256			/* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE	/* Boot Argument Buffer Size */
#define CONFIG_TFTP_FILE_NAME_MAX_LEN	256
#define SQUASHFS_MAGIC_NUMBER		"73717368"

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}

/* Digi commands arguments help */
#define DIGICMD_ARG_BLKDEV_HELP	\
	"       - device:part: number of device and partition\n"
#define DIGICMD_ARG_FILENAME_UPDATE_HELP	\
	"       - filename: file to transfer (if not provided, filename\n" \
	"                   will be taken from variable $<partition>_file)\n"
#define DIGICMD_ARG_FILENAME_DBOOT_HELP	\
	"       - filename: kernel file to transfer (if not provided, filename\n" \
	"                   will be taken from the variable pointed to by \n" \
	"                   $dboot_kernel_var)\n"
#define DIGICMD_ARG_IMGADDR_HELP	\
	"       - image_address: address of image in RAM\n" \
	"                        ($update_addr if not provided)\n"
#define DIGICMD_ARG_IMGSIZE_HELP	\
	"       - image_size: size of image in RAM\n" \
	"                    ($filesize if not provided)\n"
#define DIGICMD_ARG_SOURCEFILE_HELP	\
	"       - source_file: file to transfer\n"
#define DIGICMD_ARG_TARGETFILE_HELP	\
	"       - target_file: target filename\n"
#define DIGICMD_ARG_TARGETFILESYS_HELP	\
	"       - target_fs: fat (default)\n"
#define DIGICMD_ARG_PARTITION_HELP	\
	"       - partition: partition name (if not provided, a partition \n" \
	"                    with the name of the OS will be assumed)\n"

/* Help arguments for update command */
#define DIGICMD_UPDATE_NET_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_NET " -> " \
	"[filename]\n" \
		DIGICMD_ARG_FILENAME_UPDATE_HELP
#define DIGICMD_UPDATE_BLOCK_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_BLOCK " -> " \
	"[device:part] [filename]\n" \
		DIGICMD_ARG_BLKDEV_HELP \
		DIGICMD_ARG_FILENAME_UPDATE_HELP
#define DIGICMD_UPDATE_RAM_ARGS_HELP	\
	"      source=ram -> [image_address] [image_size]\n" \
		DIGICMD_ARG_IMGADDR_HELP \
		DIGICMD_ARG_IMGSIZE_HELP

/* Help arguments for dboot command */
#define DIGICMD_DBOOT_NET_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_NET " -> " \
	"[filename]\n" \
		DIGICMD_ARG_FILENAME_DBOOT_HELP
#define DIGICMD_DBOOT_BLOCK_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_BLOCK " -> " \
	"[device:part] [filename]\n" \
		DIGICMD_ARG_BLKDEV_HELP \
		DIGICMD_ARG_FILENAME_DBOOT_HELP
#define DIGICMD_DBOOT_NAND_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_NAND " -> " \
	"[partition] [filename]\n" \
		DIGICMD_ARG_PARTITION_HELP \
		DIGICMD_ARG_FILENAME_DBOOT_HELP

/* Help arguments for updatefile command */
#define DIGICMD_UPDATEFILE_NET_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_NET " -> " \
	"[source_file] [targetfile] [target_fs]\n" \
		DIGICMD_ARG_SOURCEFILE_HELP \
		DIGICMD_ARG_TARGETFILE_HELP \
		DIGICMD_ARG_TARGETFILESYS_HELP
#define DIGICMD_UPDATEFILE_BLOCK_ARGS_HELP	\
	"      source=" CONFIG_SUPPORTED_SOURCES_BLOCK " -> " \
	"[device:part] [source_file] [target_file] [target_fs]\n" \
		DIGICMD_ARG_BLKDEV_HELP \
		DIGICMD_ARG_SOURCEFILE_HELP \
		DIGICMD_ARG_TARGETFILE_HELP \
		DIGICMD_ARG_TARGETFILESYS_HELP
#define DIGICMD_UPDATEFILE_RAM_ARGS_HELP	\
	"      source=ram -> "\
	"[image_address] [image_size] [targetfile] [target_fs]\n" \
		DIGICMD_ARG_IMGADDR_HELP \
		DIGICMD_ARG_IMGSIZE_HELP \
		DIGICMD_ARG_TARGETFILE_HELP \
		DIGICMD_ARG_TARGETFILESYS_HELP

#ifndef __ASSEMBLY__		/* put C only stuff in this section */
/* global functions */
bool board_has_emmc(void);
#endif /* __ASSEMBLY__ */

#define DELIM_OV_FILE           ","

#endif /* __DIGI_COMMON_H */
