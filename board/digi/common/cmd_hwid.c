/*
 * (C) Copyright 2014-2018 Digi International, Inc.
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
#include "helper.h"
#include "hwid.h"

const char *cert_regions[] = {
	"U.S.A.",
	"International",
	"Japan",
};

/* Print hwid_array as HWID output */
__weak void print_hwid(u32 *hwid_array)
{
	int i;
	int cert;

	for (i = HWID_ARRAY_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", hwid_array[i]);
	printf("\n");

	/* Formatted printout */
	printf("    Year:          20%02d\n", (hwid_array[1] >> 26) & 0x3f);
	printf("    Week:          %02d\n", (hwid_array[1] >> 20) & 0x3f);
	printf("    Wireless ID:   0x%x\n", (hwid_array[1] >> 16) & 0xf);
	printf("    Variant:       0x%02x\n", (hwid_array[1] >> 8) & 0xff);
	printf("    HW Version:    0x%x\n", (hwid_array[1] >> 4) & 0xf);
	cert = hwid_array[1] & 0xf;
	printf("    Cert:          0x%x (%s)\n", cert,
	       cert < ARRAY_SIZE(cert_regions) ? cert_regions[cert] : "??");
	printf("    Location:      %c\n", ((hwid_array[0] >> 27) & 0x1f) + 'A');
	printf("    Generator ID:  %02d\n", (hwid_array[0] >> 20) & 0x7f);
	printf("    S/N:           %06d\n", hwid_array[0] & 0xfffff);
}

/* Print hwid_array as MANUFID output */
__weak void print_manufid(u32 *hwid_array)
{
	int i;

	for (i = HWID_ARRAY_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", hwid_array[i]);
	printf("\n");

	/* Formatted printout */
	printf(" Manufacturing ID: %c%02d%02d%02d%06d %02x%x%x %x\n",
	       ((hwid_array[0] >> 27) & 0x1f) + 'A',
	       (hwid_array[1] >> 26) & 0x3f,
	       (hwid_array[1] >> 20) & 0x3f,
	       (hwid_array[0] >> 20) & 0x7f,
	       hwid_array[0] & 0xfffff,
	       (hwid_array[1] >> 8) & 0xff,
	       (hwid_array[1] >> 4) & 0xf,
	       hwid_array[1] & 0xf,
	       (hwid_array[1] >> 16) & 0xf);
}

/* Parse HWID input into hwid_array */
__weak int parse_hwid(int argc, char *const argv[], u32 *hwid_array)
{
	int i;
	int word;

	if (argc != HWID_ARRAY_WORDS_NUMBER)
		return -EINVAL;

	/*
	 * Digi HWID is set as a couple of hex strings in the form
	 *     <HWID1> <HWID0>
	 * where:
	 *
	 *        | 31..26 | 25..20 | 19..16 |  15..8  | 7..4 | 3..0 |
	 *        +--------+--------+--------+---------+------+------+
	 * HWID1: |  Year  |  Week  |  WID   | Variant |  HV  | Cert |
	 *        +--------+--------+--------+---------+------+------+
	 *
	 *        |  31..27  | 26..20 |             19..0            |
	 *        +----------+--------+------------------------------+
	 * HWID0: | Location |  GenID |         Serial number        |
	 *        +----------+--------+------------------------------+
	 */

	/* Parse backwards, from MSB to LSB */
	word = HWID_ARRAY_WORDS_NUMBER - 1;
	for (i = 0; i < HWID_ARRAY_WORDS_NUMBER; i++, word--) {
		if (strtou32(argv[i], 16, &hwid_array[word]))
			return -EINVAL;
	}

	return 0;
}

/* Parse MANUFID input into hwid_array */
__weak int parse_manufid(int argc, char *const argv[], u32 *hwid_array)
{
	char tmp[13];
	unsigned long num;

	/* Initialize HWID words */
	hwid_array[0] = 0;
	hwid_array[1] = 0;

	if (argc < 2 || argc > 3)
		goto err;

	/*
	 * Digi Manufacturing team produces a string in the form
	 *     <LYYWWGGXXXXXX>
	 * where:
	 *  - L:	location, an uppercase letter [A..Z]
	 *  - YY:	year (last two digits of XXI century, in decimal)
	 *  - WW:	week of year (in decimal)
	 *  - GG:	generator ID (in decimal)
	 *  - XXXXXX:	serial number (in decimal)
	 * this information goes into the following places on the HWID:
	 *  - L:	HWID0 bits 31..27 (5 bits)
	 *  - YY:	HWID1 bits 31..26 (6 bits)
	 *  - WW:	HWID1 bits 25..20 (6 bits)
	 *  - GG:	HWID0 bits 26..20 (7 bits)
	 *  - XXXXXX:	HWID0 bits 19..0 (20 bits)
	 */
	if (strlen(argv[0]) != 13)
		goto err;

	/*
	 * A second string in the form <VVHC> must be given where:
	 *  - VV:	variant (in hex)
	 *  - H:	hardware version (in hex)
	 *  - C:	wireless certification (in hex)
	 * this information goes into the following places on the HWID:
	 *  - VV:	HWID1 bits 15..8 (8 bits)
	 *  - H:	HWID1 bits 7..4 (4 bits)
	 *  - C:	HWID1 bits 3..0 (4 bits)
	 */
	if (strlen(argv[1]) != 4)
		goto err;

	/*
	 * A third string (if provided) in the form <K> may be given, where:
	 *  - K:	wireless ID (in hex)
	 * this information goes into the following places on the HWID:
	 *  - K:	HWID1 bits 19..16 (4 bits)
	 * If not provided, a zero is used (for backwards compatibility)
	 */
	if (argc > 2) {
		if (strlen(argv[2]) != 1)
			goto err;
	}

	/* Location */
	if (argv[0][0] < 'A' || argv[0][0] > 'Z')
		goto err;
	hwid_array[0] |= (argv[0][0] - 'A') << 27;
	printf("    Location:      %c\n", argv[0][0]);

	/* Year (only 6 bits: from 0 to 63) */
	strncpy(tmp, &argv[0][1], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 0 || num > 63)
		goto err;
	hwid_array[1] |= num << 26;
	printf("    Year:          20%02d\n", (int)num);

	/* Week */
	strncpy(tmp, &argv[0][3], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 1 || num > 54)
		goto err;
	hwid_array[1] |= num << 20;
	printf("    Week:          %02d\n", (int)num);

	/* Generator ID */
	strncpy(tmp, &argv[0][5], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 0 || num > 99)
		goto err;
	hwid_array[0] |= num << 20;
	printf("    Generator ID:  %02d\n", (int)num);

	/* Serial number */
	strncpy(tmp, &argv[0][7], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 0 || num > 999999)
		goto err;
	hwid_array[0] |= num;
	printf("    S/N:           %06d\n", (int)num);

	/* Variant */
	strncpy(tmp, &argv[1][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num < 0 || num > 0xff)
		goto err;
	hwid_array[1] |= num << 8;
	printf("    Variant:       0x%02x\n", (int)num);

	/* Hardware version */
	strncpy(tmp, &argv[1][2], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num < 0 || num > 0xf)
		goto err;
	hwid_array[1] |= num << 4;
	printf("    HW version:    0x%x\n", (int)num);

	/* Cert */
	strncpy(tmp, &argv[1][3], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num < 0 || num > 0xf)
		goto err;
	hwid_array[1] |= num;
	printf("    Cert:          0x%x (%s)\n", (int)num,
	       num < ARRAY_SIZE(cert_regions) ? cert_regions[num] : "??");

	num = 0;
	if (argc > 2) {
		/* Wireless ID */
		strncpy(tmp, &argv[2][0], 1);
		tmp[1] = 0;
		num = simple_strtol(tmp, NULL, 16);
		if (num < 0 || num > 0xf)
			goto err;
		hwid_array[1] |= num << 16;
	}
	printf("    Wireless ID:   0x%x\n", (int)num);

	return 0;

err:
	printf("Invalid manufacturing string.\n"
		"Manufacturing information must be in the form: "
		CONFIG_MANUF_STRINGS_HELP "\n");
	return -EINVAL;
}

static int do_hwid(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 hwid_array[HWID_ARRAY_WORDS_NUMBER];
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "read") || !strcmp(op, "read_manuf")) {
		printf("Reading HWID: ");
		board_read_hwid(hwid_array);
		if (!strcmp(op, "read_manuf"))
			print_manufid(hwid_array);
		else
			print_hwid(hwid_array);
	} else if (!strcmp(op, "sense") || !strcmp(op, "sense_manuf")) {
		printf("Sensing HWID: ");
		board_sense_hwid(hwid_array);
		if (!strcmp(op, "sense_manuf"))
			print_manufid(hwid_array);
		else
			print_hwid(hwid_array);
	} else if (!strcmp(op, "prog")) {
		if (parse_hwid(argc, argv, hwid_array))
			return CMD_RET_USAGE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming HWID... ");
		ret = board_prog_hwid(hwid_array);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "prog_manuf")) {
		if (parse_manufid(argc, argv, hwid_array))
			return CMD_RET_FAILURE;
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Programming manufacturing information into HWID... ");
		ret = board_prog_hwid(hwid_array);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "override")) {
		if (parse_hwid(argc, argv, hwid_array))
			return CMD_RET_USAGE;
		printf("Overriding HWID... ");
		ret = board_override_hwid(hwid_array);
		if (ret)
			goto err;
		printf("OK\n");
	}  else if (!strcmp(op, "override_manuf")) {
		if (parse_manufid(argc, argv, hwid_array))
			return CMD_RET_FAILURE;
		printf("Overriding manufacturing information into HWID... ");
		ret = board_override_hwid(hwid_array);
		if (ret)
			goto err;
		printf("OK\n");
	} else if (!strcmp(op, "lock")) {
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;
		printf("Locking HWID... ");
		ret = board_lock_hwid();
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

U_BOOT_CMD(
	hwid, CONFIG_SYS_MAXARGS, 0, do_hwid,
	"HWID on fuse sub-system",
	     "read - read HWID from shadow registers\n"
	"hwid read_manuf - read HWID from shadow registers and print manufacturing ID\n"
	"hwid sense - sense HWID from fuses\n"
	"hwid sense_manuf - sense HWID from fuses and print manufacturing ID\n"
	"hwid prog [-y] <high_word> <low_word> - program HWID (PERMANENT)\n"
	"hwid prog_manuf [-y] " CONFIG_MANUF_STRINGS_HELP " - program HWID with manufacturing ID (PERMANENT)\n"
	"hwid override <high_word> <low_word> - override HWID\n"
	"hwid override_manuf " CONFIG_MANUF_STRINGS_HELP " - override HWID with manufacturing ID\n"
	"hwid lock [-y] - lock HWID OTP bits (PERMANENT)\n"
);
