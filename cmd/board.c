/*
 * (C) Copyright 2014-2017 Digi International, Inc.
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
#include "../board/digi/common/helper.h"

#if defined(CONFIG_CMD_BOARD_VERSION) && \
    defined(CONFIG_CARRIERBOARD_VERSION_ON_OTP) && \
    defined(CONFIG_HAS_CARRIERBOARD_VERSION)
__weak void print_board_version(u32 version)
{
	printf("Board: %s %s\n", CONFIG_SOM_DESCRIPTION,
	       CONFIG_BOARD_DESCRIPTION);
	if (CARRIERBOARD_VERSION_UNDEFINED == version)
		printf("       WARNING: Undefined board version!\n");
	else
		printf("       Version: %d\n", version);
}

static int do_board_version(struct cmd_tbl *cmdtp, int flag, int argc,
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

		/* Read current value of OTP word */
		ret = fuse_read(CONFIG_CARRIERBOARD_VERSION_BANK,
				CONFIG_CARRIERBOARD_VERSION_WORD, &val);
		if (ret)
			goto err;
		/* OR mask with value to write */
		val &= ~(CONFIG_CARRIERBOARD_VERSION_MASK <<
			 CONFIG_CARRIERBOARD_VERSION_OFFSET);
		val |= version;

		printf("Overriding carrier board version... ");
		ret = fuse_override(CONFIG_CARRIERBOARD_VERSION_BANK,
				    CONFIG_CARRIERBOARD_VERSION_WORD, val);
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
#endif /* CONFIG_CMD_BOARD_VERSION && CONFIG_CARRIERBOARD_VERSION_ON_OTP &&
	* CONFIG_HAS_CARRIERBOARD_VERSION */

#if defined(CONFIG_CMD_BOARD_ID) && \
    defined(CONFIG_CARRIERBOARD_ID_ON_OTP) && \
    defined(CONFIG_HAS_CARRIERBOARD_ID)
__weak void print_board_id(u32 id)
{
	printf("Board: %s %s\n", CONFIG_SOM_DESCRIPTION,
	       CONFIG_BOARD_DESCRIPTION);
	if (CARRIERBOARD_ID_UNDEFINED == id)
		printf("       WARNING: Undefined board ID!\n");
	else
		printf("       ID: %d\n", id);
}

static int do_board_id(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 val, id;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read")) {
		printf("Reading carrier board ID...\n");
		ret = fuse_read(CONFIG_CARRIERBOARD_ID_BANK,
				CONFIG_CARRIERBOARD_ID_WORD, &val);
		if (ret)
			goto err;
		id = (val >> CONFIG_CARRIERBOARD_ID_OFFSET) &
		     CONFIG_CARRIERBOARD_ID_MASK;
		print_board_id(id);
	} else if (!strcmp(op, "sense")) {
		printf("Sensing carrier board ID...\n");
		ret = fuse_sense(CONFIG_CARRIERBOARD_ID_BANK,
				 CONFIG_CARRIERBOARD_ID_WORD, &val);
		if (ret)
			goto err;
		id = (val >> CONFIG_CARRIERBOARD_ID_OFFSET) &
		     CONFIG_CARRIERBOARD_ID_MASK;
		print_board_id(id);
	} else if (!strcmp(op, "prog")) {
		if (argc < 1)
			return CMD_RET_USAGE;

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		/* Validate input value */
		strtou32(argv[0], 10, &val);
		id = val & CONFIG_CARRIERBOARD_ID_MASK;
		if (id != val) {
			printf("Provided ID %d does not fit into"
			       "carrier board ID mask: 0x%x\n", val,
			       CONFIG_CARRIERBOARD_ID_MASK);
			return CMD_RET_FAILURE;
		}
		id <<= CONFIG_CARRIERBOARD_ID_OFFSET;
		printf("Programming carrier board ID... ");
		ret = fuse_prog(CONFIG_CARRIERBOARD_ID_BANK,
				CONFIG_CARRIERBOARD_ID_WORD, id);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "override")) {
		if (argc < 1)
			return CMD_RET_USAGE;

		/* Validate input value */
		strtou32(argv[0], 10, &val);
		id = val & CONFIG_CARRIERBOARD_ID_MASK;
		if (id != val) {
			printf("Provided ID %d does not fit into"
			       "carrier board ID mask: 0x%x\n", val,
			       CONFIG_CARRIERBOARD_ID_MASK);
			return CMD_RET_FAILURE;
		}
		id <<= CONFIG_CARRIERBOARD_ID_OFFSET;

		/* Read current value of OTP word */
		ret = fuse_read(CONFIG_CARRIERBOARD_ID_BANK,
				CONFIG_CARRIERBOARD_ID_WORD, &val);
		if (ret)
			goto err;
		/* OR mask with value to write */
		val &= ~(CONFIG_CARRIERBOARD_ID_MASK <<
			 CONFIG_CARRIERBOARD_ID_OFFSET);
		val |= id;

		printf("Overriding carrier board ID... ");
		ret = fuse_override(CONFIG_CARRIERBOARD_ID_BANK,
				    CONFIG_CARRIERBOARD_ID_WORD, val);
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
	board_id, CONFIG_SYS_MAXARGS, 0, do_board_id,
	"Carrier board ID on fuse sub-system",
		      "read - read carrier board ID from shadow registers\n"
	"board_id sense - sense carrier board ID from fuses\n"
	"board_id prog [-y] <id> - program carrier board ID (PERMANENT)\n"
	"board_id override <id> - override carrier board ID\n"
	"\nNOTE: <id> parameter is in DECIMAL\n"
);
#endif /* CONFIG_CMD_BOARD_ID && CONFIG_CARRIERBOARD_ID_ON_OTP &&
	* CONFIG_HAS_CARRIERBOARD_ID */
