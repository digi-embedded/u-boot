// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2026, Digi International, Inc.
 */

#include <command.h>
#include "../board/digi/common/helper.h"
#include "../board/digi/common/smarcid.h"

static int do_smarcid_fuse(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	struct digi_smarcid smarcid;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read") || !strcmp(op, "read_manuf")) {
		printf("Reading (FUSE) SMARCID: ");
		ret = board_smarcid_fuse_read(&smarcid);
		if (ret)
			goto fuse_err;
		if (!strcmp(op, "read_manuf"))
			board_smarcid_print_manuf(&smarcid);
		else
			board_smarcid_print(&smarcid);
	} else if (!strcmp(op, "prog")) {
		if (board_smarcid_parse(argc, argv, &smarcid))
			return CMD_RET_USAGE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming (FUSE) SMARCID... ");
		ret = board_smarcid_fuse_prog(&smarcid);
		if (ret)
			goto fuse_err;
		printf("OK\n");
	} else if (!strcmp(op, "prog_manuf")) {
		if (board_smarcid_parse_manuf(argc, argv, &smarcid))
			return CMD_RET_FAILURE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming manufacturing information into (FUSE) SMARCID... ");
		ret = board_smarcid_fuse_prog(&smarcid);
		if (ret)
			goto fuse_err;
		printf("OK\n");
	} else if (!strcmp(op, "lock")) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking (FUSE) SMARCID... ");
		ret = board_smarcid_fuse_lock();
		if (ret)
			goto fuse_err;
		printf("OK\n");
		printf("Locking of the SMARCID will be effective when the CPU is reset\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

fuse_err:
	puts("ERROR\n");
	return CMD_RET_FAILURE;
}

static int do_smarcid_env(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	struct digi_smarcid smarcid;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2;
	argv += 2;

	if (!strcmp(op, "read") || !strcmp(op, "read_manuf")) {
		printf("Reading (ENV) SMARCID: ");
		ret = smarcid_env_read(&smarcid);
		if (ret) {
			printf("ERROR: Undefined environment SMARCID\n");
			return CMD_RET_USAGE;
		}
		if (!strcmp(op, "read_manuf"))
			board_smarcid_print_manuf(&smarcid);
		else
			board_smarcid_print(&smarcid);
	} else if (!strcmp(op, "prog")) {
		if (board_smarcid_parse(argc, argv, &smarcid))
			return CMD_RET_USAGE;
		printf("Programming (ENV) SMARCID... ");
		ret = smarcid_env_prog(&smarcid);
		if (ret)
			goto env_err;
		printf("OK\n");
	} else if (!strcmp(op, "prog_manuf")) {
		if (board_smarcid_parse_manuf(argc, argv, &smarcid))
			return CMD_RET_FAILURE;
		printf("Programming manufacturing information into (ENV) SMARCID... ");
		ret = smarcid_env_prog(&smarcid);
		if (ret)
			goto env_err;
		printf("OK\n");
	} else if (!strcmp(op, "clear")) {
		printf("Clearing (ENV) SMARCID... ");
		ret = smarcid_env_clear();
		if (ret)
			goto env_err;
		printf("OK\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

env_err:
	puts("ERROR\n");
	return CMD_RET_FAILURE;
}

U_BOOT_CMD_WITH_SUBCMDS(smarcid, "SMARCID",
	        "fuse read - read SMARCID from fuse registers\n" \
	"smarcid fuse read_manuf - read SMARCID from fuse registers and print manufacturing ID\n" \
	"smarcid fuse prog [-y] " CONFIG_SMARCID_STRINGS_HELP " - program SMARCID into fuse registers (PERMANENT)\n" \
	"smarcid fuse prog_manuf [-y] " CONFIG_SMARCID_MANUF_STRINGS_HELP " - program SMARCID with manufacturing ID into fuse registers (PERMANENT)\n" \
	"smarcid fuse lock [-y] - lock SMARCID OTP bits (PERMANENT)\n" \
	"smarcid env read - read SMARCID from the environment\n" \
	"smarcid env read_manuf - read SMARCID from the environment and print manufacturing ID\n" \
	"smarcid env prog " CONFIG_SMARCID_STRINGS_HELP " - program SMARCID into the environment\n" \
	"smarcid env prog_manuf " CONFIG_SMARCID_MANUF_STRINGS_HELP " - program SMARCID with manufacturing ID into the environment\n" \
	"smarcid env clear - clear SMARCID from the environment\n"
	,
	U_BOOT_SUBCMD_MKENT(fuse, CONFIG_SYS_MAXARGS, 0, do_smarcid_fuse),
	U_BOOT_SUBCMD_MKENT(env, CONFIG_SYS_MAXARGS, 0, do_smarcid_env));
