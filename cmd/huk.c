// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Digi International, Inc.
 */

#include <common.h>
#include <command.h>
#include <stdlib.h>
#include <linux/errno.h>
#include "../board/digi/common/helper.h"
#include "../board/digi/common/huk.h"

int huk_otp_id = 0;
int huk_words = 0;

__weak void board_parse_dt_huk(int *huk_otp_id, int *huk_words) {}

static int do_huk(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	uint32_t *huk;
	int i;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	board_parse_dt_huk(&huk_otp_id, &huk_words);
	if (!huk_words) {
		printf("Error: couldn't get HUK info from DT\n");
		return CMD_RET_FAILURE;
	} else if (huk_words > MAX_HUK_OTP_WORDS) {
		printf("Error: defined HUK OTP words (%d) exceed max (%d)\n",
			huk_words, MAX_HUK_OTP_WORDS);
		return CMD_RET_FAILURE;
	}

	if (!strcmp(op, "prog")) {
		if (huk_words != argc) {
			printf("Error: wrong number of HUK words (expected %d)\n",
				huk_words);
			return CMD_RET_FAILURE;
		}

		huk = malloc(sizeof(uint32_t) * huk_words);
		if (!huk) {
			printf("Error: cannot allocate memory\n");
			return CMD_RET_FAILURE;
		}

		for (i = 0; i < huk_words; i++) {
			if (strtou32(argv[i], 16, &huk[i])) {
				printf("Error: cannot parse HUK%d word\n", i + 1);
				return CMD_RET_FAILURE;
			}
			printf("HUK%d: 0x%08x\n", i + 1, huk[i]);
		}

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		printf("Programming HUK... ");
		ret = board_prog_huk(huk_otp_id, huk_words, huk);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "lock")) {
		printf("Words ");
		for (i = 0; i < huk_words; i++)
			printf("%d ", huk_otp_id + i);
		printf("will be locked\n");
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking HUK... ");
		ret = board_lock_huk(huk_otp_id, huk_words);
		if (ret)
			goto err;
		printf("OK\n");
		printf("Locking of the HUK will be effective when the CPU is reset\n");
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("ERROR\n");
	return ret;
}

U_BOOT_CMD(
	huk, CONFIG_SYS_MAXARGS, 0, do_huk,
	"HUK on fuse sub-system",
	     "prog [-y] <HUK1> <HUK2> <HUK3> <HUK4> - program HUK (PERMANENT)\n" \
	"huk lock [-y] - lock HUK OTP bits (PERMANENT)\n"
);
