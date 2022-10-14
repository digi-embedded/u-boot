/*
 * Copyright 2022 Digi International Inc
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#ifndef CCIMX93_COMMON_H
#define CCIMX93_COMMON_H

#include "digi_common.h"

/* Supported sources for update|dboot */
#if defined(CONFIG_CMD_DBOOT) || defined(CONFIG_CMD_UPDATE)
#define CONFIG_SUPPORTED_SOURCES	((1 << SRC_TFTP) | \
					 (1 << SRC_NFS) | \
					 (1 << SRC_MMC) | \
					 (1 << SRC_USB) | \
					 (1 << SRC_RAM))
#define CONFIG_SUPPORTED_SOURCES_NET	"tftp|nfs"
#define CONFIG_SUPPORTED_SOURCES_BLOCK	"mmc|usb"
#define CONFIG_SUPPORTED_SOURCES_RAM	"ram"
#endif /* CONFIG_CMD_DBOOT || CONFIG_CMD_UPDATE */

/* Digi direct boot command 'dboot' */
#ifdef CONFIG_CMD_DBOOT
#define CONFIG_DBOOT_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK
#define CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_DBOOT_NET_ARGS_HELP "\n" \
	DIGICMD_DBOOT_BLOCK_ARGS_HELP
#endif /* CONFIG_CMD_DBOOT */

#endif /* CCIMX93_COMMON_H */
