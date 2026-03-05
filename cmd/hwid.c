/*
 * (C) Copyright 2014-2026, Digi International, Inc.
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
#include <linux/errno.h>
#include "../board/digi/common/helper.h"
#include "../board/digi/common/hwid.h"

static int do_hwid_fuse(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	struct digi_hwid hwid;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read") || !strcmp(op, "read_manuf")) {
		printf("Reading (FUSE) HWID: ");
		board_hwid_fuse_read(&hwid);
		if (!strcmp(op, "read_manuf"))
			board_hwid_print_manuf(&hwid);
		else
			board_hwid_print(&hwid);
	} else if (!strcmp(op, "prog")) {
		if (board_hwid_parse(argc, argv, &hwid))
			return CMD_RET_USAGE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming (FUSE) HWID... ");
		ret = board_hwid_fuse_prog(&hwid);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "prog_manuf")) {
		if (board_hwid_parse_manuf(argc, argv, &hwid))
			return CMD_RET_FAILURE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming manufacturing information into (FUSE) HWID... ");
		ret = board_hwid_fuse_prog(&hwid);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "lock")) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking (FUSE) HWID... ");
		ret = board_hwid_fuse_lock();
		if (ret)
			goto err;
		printf("OK\n");
		printf("Locking of the HWID will be effective when the CPU is reset\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("ERROR\n");
	return ret;
}

static int do_hwid_env(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	struct digi_hwid hwid;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2;
	argv += 2;

	if (!strcmp(op, "read") || !strcmp(op, "read_manuf")) {
		printf("Reading (ENV) HWID: ");
		ret = hwid_env_read(&hwid);
		if (ret) {
			printf("ERROR: Undefined environment HWID\n");
			return CMD_RET_USAGE;
		}
		if (!strcmp(op, "read_manuf"))
			board_hwid_print_manuf(&hwid);
		else
			board_hwid_print(&hwid);
	} else if (!strcmp(op, "prog")) {
		if (board_hwid_parse(argc, argv, &hwid))
			return CMD_RET_USAGE;
		printf("Programming (ENV) HWID... ");
		ret = hwid_env_prog(&hwid);
		if (ret)
			goto err_env;
		printf("OK\n");
	} else if (!strcmp(op, "prog_manuf")) {
		if (board_hwid_parse_manuf(argc, argv, &hwid))
			return CMD_RET_FAILURE;
		printf("Programming manufacturing information into (ENV) HWID... ");
		ret = hwid_env_prog(&hwid);
		if (ret)
			goto err_env;
		printf("OK\n");
	} else if (!strcmp(op, "clear")) {
		printf("Clearing (ENV) HWID... ");
		ret = hwid_env_clear();
		if (ret)
			goto err_env;
		printf("OK\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err_env:
	puts("ERROR\n");
	return CMD_RET_FAILURE;
}

U_BOOT_CMD_WITH_SUBCMDS(hwid, "HWID",
	     "fuse read - read HWID from fuse registers\n" \
	"hwid fuse read_manuf - read HWID from fuse registers and print manufacturing ID\n" \
	"hwid fuse prog [-y] " CONFIG_HWID_STRINGS_HELP " - program HWID into fuse registers (PERMANENT)\n" \
	"hwid fuse prog_manuf [-y] " CONFIG_MANUF_STRINGS_HELP " - program HWID with manufacturing ID into fuse registers (PERMANENT)\n" \
	"hwid fuse lock [-y] - lock HWID OTP bits (PERMANENT)\n" \
	"hwid env read - read HWID from the environment\n" \
	"hwid env read_manuf - read HWID from the environment and print manufacturing ID\n" \
	"hwid env prog " CONFIG_HWID_STRINGS_HELP " - program HWID into the environment\n" \
	"hwid env prog_manuf " CONFIG_MANUF_STRINGS_HELP " - program HWID with manufacturing ID into the environment\n" \
	"hwid env clear - clear HWID from the environment\n"
	,
	U_BOOT_SUBCMD_MKENT(fuse, CONFIG_SYS_MAXARGS, 0, do_hwid_fuse),
	U_BOOT_SUBCMD_MKENT(env, CONFIG_SYS_MAXARGS, 0, do_hwid_env));
