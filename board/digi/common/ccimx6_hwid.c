/*
 * Copyright (C) 2016-2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <linux/errno.h>
#include <fuse.h>
#include <fdt_support.h>
#include "helper.h"
#include "hwid.h"

extern struct digi_hwid my_hwid;

const char *cert_regions[] = {
	"U.S.A.",
	"International",
	"Japan",
};

/* Print HWID info */
void board_print_hwid(struct digi_hwid *hwid)
{
	int i;

	for (i = CONFIG_HWID_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", ((u32 *)hwid)[i]);
	printf("\n");

	/* Formatted printout */
	printf("    Year:          20%02d\n", hwid->year);
	printf("    Week:          %02d\n", hwid->week);
	printf("    Wireless ID:   0x%x\n", hwid->wid);
	printf("    Variant:       0x%02x\n", hwid->variant);
	printf("    HW Version:    0x%x\n", hwid->hv);
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
		hwid->cert < ARRAY_SIZE(cert_regions) ?
		cert_regions[hwid->cert] : "??");
	printf("    Location:      %c\n", hwid->location + 'A');
	printf("    Generator ID:  %02d\n", hwid->genid);
	printf("    S/N:           %06d\n", hwid->sn);
}

/* Print HWID info in MANUFID format */
void board_print_manufid(struct digi_hwid *hwid)
{
	int i;

	for (i = CONFIG_HWID_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", ((u32 *)hwid)[i]);
	printf("\n");

	/* Formatted printout */
	printf(" Manufacturing ID: %c%02d%02d%02d%06d %02x%x%x %x\n",
		hwid->location + 'A',
		hwid->year,
		hwid->week,
		hwid->genid,
		hwid->sn,
		hwid->variant,
		hwid->hv,
		hwid->cert,
		hwid->wid);
}

/* Parse HWID info in HWID format */
int board_parse_hwid(int argc, char *const argv[], struct digi_hwid *hwid)
{
	int i, word;
	u32 hwidword;

	if (argc != CONFIG_HWID_WORDS_NUMBER)
		goto err;

	/*
	 * Digi HWID is set as a couple of hex strings in the form
	 *     <high word> <low word>
	 * that are inversely stored into the structure.
	 */

	/* Parse backwards, from MSB to LSB */
	word = CONFIG_HWID_WORDS_NUMBER - 1;
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word--) {
		if (strtou32(argv[i], 16, &hwidword))
			goto err;

		((u32 *)hwid)[word] = hwidword;
	}

	return 0;

err:
	printf("Invalid HWID input.\n"
		"HWID input must be in the form: "
		CONFIG_HWID_STRINGS_HELP "\n");
	return -EINVAL;
}

/* Parse HWID info in MANUFID format */
int board_parse_manufid(int argc, char *const argv[], struct digi_hwid *hwid)
{
	char tmp[13];
	unsigned long num;

	/* Initialize HWID words */
	memset(hwid, 0, sizeof(struct digi_hwid));

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
	 * this information goes into the following places on the fuses:
	 *  - L:	OCOTP_MAC0 bits 31..27 (5 bits)
	 *  - YY:	OCOTP_MAC1 bits 31..26 (6 bits)
	 *  - WW:	OCOTP_MAC1 bits 25..20 (6 bits)
	 *  - GG:	OCOTP_MAC0 bits 26..20 (7 bits)
	 *  - XXXXXX:	OCOTP_MAC0 bits 19..0 (20 bits)
	 */
	if (strlen(argv[0]) != 13)
		goto err;

	/*
	 * A second string in the form <VVHC> must be given where:
	 *  - VV:	variant (in hex)
	 *  - H:	hardware version (in hex)
	 *  - C:	wireless certification (in hex)
	 * this information goes into the following places on the fuses:
	 *  - VV:	OCOTP_MAC1 bits 15..8 (8 bits)
	 *  - H:	OCOTP_MAC1 bits 7..4 (4 bits)
	 *  - C:	OCOTP_MAC1 bits 3..0 (4 bits)
	 */
	if (strlen(argv[1]) != 4)
		goto err;

	/*
	 * A third string (if provided) in the form <K> may be given, where:
	 *  - K:	wireless ID (in hex)
	 * this information goes into the following places on the fuses:
	 *  - K:	OCOTP_MAC1 bits 19..16 (4 bits)
	 * If not provided, a zero is used (for backwards compatibility)
	 */
	if (argc > 2) {
		if (strlen(argv[2]) != 1)
			goto err;
	}

	/* Location */
	if (argv[0][0] < 'A' || argv[0][0] > 'Z')
		goto err;
	hwid->location = (argv[0][0] - 'A');
	printf("    Location:      %c\n", hwid->location + 'A');

	/* Year (only 6 bits: from 0 to 63) */
	strncpy(tmp, &argv[0][1], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 63)
		goto err;
	hwid->year = num;
	printf("    Year:          20%02d\n", hwid->year);

	/* Week */
	strncpy(tmp, &argv[0][3], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 1 || num > 54)
		goto err;
	hwid->week = num;
	printf("    Week:          %02d\n", hwid->week);

	/* Generator ID */
	strncpy(tmp, &argv[0][5], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 99)
		goto err;
	hwid->genid = num;
	printf("    Generator ID:  %02d\n", hwid->genid);

	/* Serial number */
	strncpy(tmp, &argv[0][7], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 999999)
		goto err;
	hwid->sn = num;
	printf("    S/N:           %06d\n", hwid->sn);

	/* Variant */
	strncpy(tmp, &argv[1][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0xff)
		goto err;
	hwid->variant = num;
	printf("    Variant:       0x%02x\n", hwid->variant);

	/* Hardware version */
	strncpy(tmp, &argv[1][2], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0xf)
		goto err;
	hwid->hv = num;
	printf("    HW version:    0x%x\n", hwid->hv);

	/* Cert */
	strncpy(tmp, &argv[1][3], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0xf)
		goto err;
	hwid->cert = num;
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
		hwid->cert < ARRAY_SIZE(cert_regions) ?
		cert_regions[hwid->cert] : "??");

	if (argc > 2) {
		/* Wireless ID */
		strncpy(tmp, &argv[2][0], 1);
		tmp[1] = 0;
		num = simple_strtol(tmp, NULL, 16);
		if (num < 0 || num > 0xf)
			goto err;
		hwid->wid = num;
	}
	printf("    Wireless ID:   0x%x\n", hwid->wid);

	return 0;

err:
	printf("Invalid manufacturing string.\n"
		"Manufacturing information must be in the form: "
		CONFIG_MANUF_STRINGS_HELP "\n");
	return -EINVAL;
}

int board_lock_hwid(void)
{
	return fuse_prog(OCOTP_LOCK_BANK, OCOTP_LOCK_WORD,
			CONFIG_HWID_LOCK_FUSE);
}

void fdt_fixup_hwid(void *fdt)
{
	const char *propnames[] = {
		"digi,hwid,location",
		"digi,hwid,genid",
		"digi,hwid,sn",
		"digi,hwid,year",
		"digi,hwid,week",
		"digi,hwid,variant",
		"digi,hwid,hv",
		"digi,hwid,cert",
		"digi,hwid,wid",
	};
	char str[20];
	int i;

	/* Re-read HWID which might have been overridden by user */
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return;
	}

	/* Register the HWID as main node properties in the FDT */
	for (i = 0; i < ARRAY_SIZE(propnames); i++) {
		/* Convert HWID fields to strings */
		if (!strcmp("digi,hwid,location", propnames[i]))
			sprintf(str, "%c", my_hwid.location + 'A');
		else if (!strcmp("digi,hwid,genid", propnames[i]))
			sprintf(str, "%02d", my_hwid.genid);
		else if (!strcmp("digi,hwid,sn", propnames[i]))
			sprintf(str, "%06d", my_hwid.sn);
		else if (!strcmp("digi,hwid,year", propnames[i]))
			sprintf(str, "20%02d", my_hwid.year);
		else if (!strcmp("digi,hwid,week", propnames[i]))
			sprintf(str, "%02d", my_hwid.week);
		else if (!strcmp("digi,hwid,variant", propnames[i]))
			sprintf(str, "0x%02x", my_hwid.variant);
		else if (!strcmp("digi,hwid,hv", propnames[i]))
			sprintf(str, "0x%x", my_hwid.hv);
		else if (!strcmp("digi,hwid,cert", propnames[i]))
			sprintf(str, "0x%x", my_hwid.cert);
		else if (!strcmp("digi,hwid,wid", propnames[i]))
			sprintf(str, "0x%x", my_hwid.wid);
		else
			continue;

		do_fixup_by_path(fdt, "/", propnames[i], str,
				strlen(str) + 1, 1);
	}
}
