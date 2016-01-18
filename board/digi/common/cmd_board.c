/*
 * (C) Copyright 2014 Digi International, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <fuse.h>
#include <asm/errno.h>
#include "helper.h"

#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
__weak void print_board_version(u32 version)
{
	printf("Carrier board: %s ", CONFIG_BOARD_DESCRIPTION);
	if (CARRIERBOARD_VERSION_UNDEFINED == version)
		printf("(undefined version)\n");
	else
		printf("v%d\n", version);
}

static int do_board_version(cmd_tbl_t *cmdtp, int flag, int argc,
			    char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 val, version;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read")) {
		printf("Reading carrier board version...\n");
		ret = fuse_read(CONFIG_CARRIERBOARD_VERSION_BANK,
				CONFIG_CARRIERBOARD_VERSION_WORD, &val);
		if (ret)
			goto err;
		version = (val >> CONFIG_CARRIERBOARD_VERSION_OFFSET) &
			  CONFIG_CARRIERBOARD_VERSION_MASK;
		print_board_version(version);
	} else if (!strcmp(op, "sense")) {
		printf("Sensing carrier board version...\n");
		ret = fuse_sense(CONFIG_CARRIERBOARD_VERSION_BANK,
				 CONFIG_CARRIERBOARD_VERSION_WORD, &val);
		if (ret)
			goto err;
		version = (val >> CONFIG_CARRIERBOARD_VERSION_OFFSET) &
			  CONFIG_CARRIERBOARD_VERSION_MASK;
		print_board_version(version);
	} else if (!strcmp(op, "prog")) {
		if (argc < 1)
			return CMD_RET_USAGE;

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		/* Validate input value */
		strtou32(argv[0], 10, &val);
		version = val & CONFIG_CARRIERBOARD_VERSION_MASK;
		if (version != val) {
			printf("Provided version %d does not fit into"
			       "carrier board version mask: 0x%x\n", val,
			       CONFIG_CARRIERBOARD_VERSION_MASK);
			return CMD_RET_FAILURE;
		}
		version <<= CONFIG_CARRIERBOARD_VERSION_OFFSET;
		printf("Programming carrier board version... ");
		ret = fuse_prog(CONFIG_CARRIERBOARD_VERSION_BANK,
				CONFIG_CARRIERBOARD_VERSION_WORD, version);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "override")) {
		if (argc < 1)
			return CMD_RET_USAGE;

		/* Validate input value */
		strtou32(argv[0], 10, &val);
		version = val & CONFIG_CARRIERBOARD_VERSION_MASK;
		if (version != val) {
			printf("Provided version %d does not fit into"
			       "carrier board version mask: 0x%x\n", val,
			       CONFIG_CARRIERBOARD_VERSION_MASK);
			return CMD_RET_FAILURE;
		}
		version <<= CONFIG_CARRIERBOARD_VERSION_OFFSET;
		printf("Overriding carrier board version... ");
		ret = fuse_override(CONFIG_CARRIERBOARD_VERSION_BANK,
				    CONFIG_CARRIERBOARD_VERSION_WORD, version);
		if (ret)
			goto err;
		printf("OK\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("ERROR\n");
	return ret;
}

U_BOOT_CMD(
	board_version, CONFIG_SYS_MAXARGS, 0, do_board_version,
	"Carrier board version on fuse sub-system",
		      "read - read carrier board version from shadow registers\n"
	"board_version sense - sense carrier board version from fuses\n"
	"board_version prog [-y] <version> - program carrier board version (PERMANENT)\n"
	"board_version override <version> - override carrier board version\n"
	"\nNOTE: <version> parameter is in DECIMAL\n"
);
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */
