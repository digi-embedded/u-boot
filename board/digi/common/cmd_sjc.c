/*
 * (C) Copyright 2016 Digi International, Inc.
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

void board_print_sjc(u32 *sjc)
{
	u32 sjc_mode;

	printf(" %.8x\n", sjc[0]);

	/* Formatted printout */
	if ((sjc_mode = (sjc[0] >> SJC_DISABLE_OFFSET) & 0x01)){
		/* Read SJC_DISABLE */
		printf("    Secure JTAG disabled\n");
	} else {
		/* read JTAG_SMODE */
		sjc_mode = (sjc[0] >> JTAG_SMODE_OFFSET) & 0x03;
		if (sjc_mode == 0)
			printf("    JTAG enable mode\n");
		else if (sjc_mode == 1)
			printf("    Secure JTAG mode\n");
		else
			printf("    No debug mode\n");
	}
}

void board_print_sjc_key(u32 *sjc)
{
	int i;

	for (i = CONFIG_SJC_KEY_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", sjc[i]);
	printf("\n");

	/* Formatted printout */
	printf("    Secure JTAG response Key: 0x%x%x\n", sjc[1], sjc[0]);
}

static int do_sjc(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 bank = CONFIG_SJC_MODE_BANK;
	u32 word = CONFIG_SJC_MODE_START_WORD;
	u32 val[2];
	int ret, i;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read")) {
		printf("Reading Secure JTAG mode: ");
		ret = fuse_read(bank, word, &val[0]);
		if (ret)
			goto err;
		board_print_sjc(val);
	} else if (!strcmp(op, "read_key")) {
		bank = CONFIG_SJC_KEY_BANK;
		word = CONFIG_SJC_KEY_START_WORD;
		printf("Reading response key: ");
		for (i = 0; i < CONFIG_SJC_KEY_WORDS_NUMBER; i++, word++) {
			ret = fuse_read(bank, word, &val[i]);
			if (ret)
				goto err;
		}
		board_print_sjc_key(val);
	} else if (!strcmp(op, "sense")) {
		printf("Sensing Secure JTAG: ");
		ret = fuse_sense(bank, word, &val[0]);
		if (ret)
			goto err;
		board_print_sjc(val);
	} else if (!strcmp(op, "sense_key")) {
		bank = CONFIG_SJC_KEY_BANK;
		word = CONFIG_SJC_KEY_START_WORD;
		printf("Sensing response key: ");
		for (i = 0; i < CONFIG_SJC_KEY_WORDS_NUMBER; i++, word++) {
			ret = fuse_sense(bank, word, &val[i]);
			if (ret)
				goto err;
		}
		board_print_sjc_key(val);
	} else if ((!strcmp(op, "prog")) || (!strcmp(op, "prog_disable"))) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		if (argc < 1)
			return CMD_RET_USAGE;

		printf("Programming Secure JTAG mode... ");
		if (strtou32(argv[0], 0, &val[0]))
			return CMD_RET_USAGE;

		if (!strcmp(op, "prog")) {
			if (val[0] == 0 || val[0] == 1 || val[0] == 3)
				ret = fuse_prog(bank, word, val[0] << JTAG_SMODE_OFFSET);
			else {
				printf("\nWrong parameter.\n");
				return CMD_RET_USAGE;
			}
		} else {
			if ((val[0] == 0) || (val[0] == 1))
				ret = fuse_prog(bank, word, val[0] << SJC_DISABLE_OFFSET);
			else {
				printf("\nWrong parameter.\n");
				return CMD_RET_USAGE;
			}
		}
		if (ret)
			goto err;
		printf("[OK]\n");
	} else if (!strcmp(op, "prog_key")) {
		if (argc < CONFIG_SJC_KEY_WORDS_NUMBER)
			return CMD_RET_USAGE;

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming response key... ");
		/* Write backwards, from MSB to LSB */
		word = CONFIG_SJC_KEY_START_WORD + CONFIG_SJC_KEY_WORDS_NUMBER - 1;
		bank = CONFIG_SJC_KEY_BANK;
		for (i = 0; i < CONFIG_SJC_KEY_WORDS_NUMBER; i++, word--) {
			if (strtou32(argv[i], 16, &val[i])) {
				ret = CMD_RET_USAGE;
				goto err;
			}

			ret = fuse_prog(bank, word, val[i]);
			if (ret)
				goto err;
		}
		printf("[OK]\n");
	} else if ((!strcmp(op, "override")) || (!strcmp(op, "override_disable"))) {
		if (argc < 1)
			return CMD_RET_USAGE;

		if (strtou32(argv[0], 0, &val[0]))
			return CMD_RET_USAGE;

		printf("Overriding Secure JTAG mode... ");
		if (!strcmp(op, "override")) {
			if (val[0] == 0 || val[0] == 1 || val[0] == 3)
				ret = fuse_override(bank, word, val[0] << JTAG_SMODE_OFFSET);
			else {
				printf("\nWrong parameter.\n");
				return CMD_RET_USAGE;
			}
		} else {
			if ((val[0] == 0) || (val[0] == 1))
				ret = fuse_override(bank, word, val[0] << SJC_DISABLE_OFFSET);
			else {
				printf("\nWrong parameter.\n");
				return CMD_RET_USAGE;
			}
		}
		if (ret)
			goto err;
		printf("[OK]\n");
	} else if (!strcmp(op, "override_key")) {
		if (argc < CONFIG_SJC_KEY_WORDS_NUMBER)
			return CMD_RET_USAGE;

		printf("Overriding response key... ");
		/* Write backwards, from MSB to LSB */
		word = CONFIG_SJC_KEY_START_WORD + CONFIG_SJC_KEY_WORDS_NUMBER - 1;
		bank = CONFIG_SJC_KEY_BANK;
		for (i = 0; i < CONFIG_SJC_KEY_WORDS_NUMBER; i++, word--) {
			if (strtou32(argv[i], 16, &val[i])) {
				ret = CMD_RET_USAGE;
				goto err;
			}

			ret = fuse_override(bank, word, val[i]);
			if (ret)
				goto err;
		}
		printf("[OK]\n");
	} else if (!strcmp(op, "lock")) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking Secure JTAG mode... ");
		ret = fuse_prog(OCOTP_LOCK_BANK,
				OCOTP_LOCK_WORD,
				CONFIG_SJC_LOCK_FUSE);
		if (ret)
			goto err;
		printf("[OK]\n");
		printf("Locking of the Secure JTAG will be effective when the CPU is reset\n");
	} else if (!strcmp(op, "lock_key")) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking response key... ");
		ret = fuse_prog(OCOTP_LOCK_BANK,
				OCOTP_LOCK_WORD,
				CONFIG_SJC_KEY_LOCK_FUSE);
		if (ret)
			goto err;
		printf("[OK]\n");
		printf("Locking of the response key will be effective when the CPU is reset\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("[ERROR]\n");
	return ret;
}

U_BOOT_CMD(
	sjc, CONFIG_SYS_MAXARGS, 0, do_sjc,
	"Secure JTAG on fuse sub-system",
	     "read - read Secure JTAG mode from shadow registers\n"
	"sjc read_key - read Secure JTAG response key from shadow registers\n"
	"sjc sense - sense Secure JTAG mode from fuses\n"
	"sjc sense_key - sense Secure JTAG response key from fuses\n"
	"sjc prog [-y] - program Secure JTAG mode <mode> (PERMANENT). <mode> can be one of:\n"
	"    0 - JTAG enable mode\n"
	"    1 - Secure JTAG mode\n"
	"    3 - No debug mode\n"
	"sjc prog_key [-y] <high_word> <low_word> - program response key (PERMANENT)\n"
	"sjc prog_disable [-y] - program disable JTAG interface OTP bit <sjc> (PERMANENT). <sjc> can be one of:\n"
	"    0 - Secure JTAG Controller is enabled\n"
	"    1 - Secure JTAG Controller is disabled\n"	
	"sjc override - override Secure JTAG mode <mode>. <mode> can be one of:\n"
	"    0 - JTAG enable mode\n"
	"    1 - Secure JTAG mode\n"
	"    3 - No debug mode\n"
	"sjc override_key <high_word> <low_word> - override response key\n"
	"sjc override_disable - override disable JTAG interface <sjc>. <sjc> can be one of:\n"
	"    0 - Secure JTAG Controller is enabled\n"
	"    1 - Secure JTAG Controller is disabled\n"
	"sjc lock [-y] - lock Secure JTAG mode and disable JTAG interface\n"
	"    OTP bits (PERMANENT)\n"
	"sjc lock_key [-y] - lock Secure JTAG key OTP bits (PERMANENT)\n"
);
