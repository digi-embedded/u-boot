/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdt_support.h>
#include <fuse.h>
#include "helper.h"
#include "hwid.h"

extern struct digi_hwid my_hwid;

const char *cert_regions[] = {
	"U.S.A.",
	"International",
	"Japan",
};

/* Program HWID into efuses */
int board_prog_hwid(const struct digi_hwid *hwid)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	u32 fuseword;
	int ret, i;

	fuse_allow_prog(true);

	for (i = 0; i < cnt; i++, word++) {
		fuseword = ((u32 *)hwid)[i];
		ret = fuse_prog(bank, word, fuseword);
		if (ret)
			break;
	}

	fuse_allow_prog(false);

	return ret;
}

/* Print HWID info */
void board_print_hwid(struct digi_hwid *hwid)
{
	printf(" %.8x", ((u32 *)hwid)[2]);
	printf(" %.4x", ((u32 *)hwid)[1]);
	printf(" %.8x", ((u32 *)hwid)[0]);
	printf("\n");

	/* Formatted printout */
	printf("    Generator ID:  %02d\n", hwid->genid);
	printf("    MAC Pool:      %02d\n", hwid->mac_pool);
	printf("    MAC Base:      %.2x:%.2x:%.2x\n",
		(hwid->mac_base >> 16) & 0xFF,
		(hwid->mac_base >> 8) & 0xFF,
		(hwid->mac_base) & 0xFF);
	printf("    Variant:       0x%02x\n", hwid->variant);
	printf("    HW Version:    0x%x\n", hwid->hv);
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
		hwid->cert < ARRAY_SIZE(cert_regions) ?
		cert_regions[hwid->cert] : "??");
	printf("    Wireless ID:   0x%x\n", hwid->wid);
	printf("    Year:          20%02d\n", hwid->year);
	printf("    Month:         %02d\n", hwid->month);
	printf("    S/N:           %06d\n", hwid->sn);
}

/* Print HWID info in MANUFID format */
void board_print_manufid(struct digi_hwid *hwid)
{
	printf(" %.8x", ((u32 *)hwid)[2]);
	printf(" %.4x", ((u32 *)hwid)[1]);
	printf(" %.8x", ((u32 *)hwid)[0]);
	printf("\n");

	/* Formatted printout */
	printf(" Manufacturing ID: %02d%02d%02d%06d %02d%06x %02x%x%x %x\n",
		hwid->year,
		hwid->month,
		hwid->genid,
		hwid->sn,
		hwid->mac_pool,
		hwid->mac_base,
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

	if (strlen(argv[0]) != 8)
		goto err;

	if (strlen(argv[1]) != 4)
		goto err;

	if (strlen(argv[2]) != 8)
		goto err;
	/*
	 * Digi HWID is set as a three hex strings in the form
	 *     <XXXXXXXX> <YYYY> <ZZZZZZZZ>
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

	if (argc < 3 || argc > 4)
		goto err;

	/*
	 * Digi Manufacturing team produces a string in the form
	 *     <YYMMGGXXXXXX>
	 * where:
	 *  - YY:	year (last two digits of XXI century, in decimal)
	 *  - MM:	month of the year (in decimal)
	 *  - GG:	generator ID (in decimal)
	 *  - XXXXXX:	serial number (in decimal)
	 * this information goes into the following places on the fuses:
	 *  - YY:	MAC1_ADDR0 bits 29..24 (6 bits)
	 *  - MM:	MAC1_ADDR0 bits 23..20 (4 bits)
	 *  - GG:	MAC2_ADDR0 bits 31..28 (4 bits)
	 *  - XXXXXX:	MAC1_ADDR0 bits 19..0 (20 bits)
	 */
	if (strlen(argv[0]) != 12)
		goto err;

	/*
	 * A second string in the form <PPAAAAAA> must be given where:
	 *  - PP:	MAC pool (in decimal)
	 *  - AAAAAA:	MAC base address (in hex)
	 * this information goes into the following places on the fuses:
	 *  - PP:	MAC2_ADDR0 bits 27..24 (4 bits)
	 *  - AAAAAA:	MAC2_ADDR0 bits 23..0 (24 bits)
	 */
	if (strlen(argv[1]) != 8)
		goto err;

	/*
	 * A third string in the form <VVHC> must be given where:
	 *  - VV:	variant (in hex)
	 *  - H:	hardware version (in hex)
	 *  - C:	wireless certification (in hex)
	 * this information goes into the following places on the fuses:
	 *  - VV:	MAC1_ADDR1 bits 10..6 (5 bits)
	 *  - H:	MAC1_ADDR1 bits 5..3 (3 bits)
	 *  - C:	MAC1_ADDR1 bits 2..0 (3 bits)
	 */
	if (strlen(argv[2]) != 4)
		goto err;

	/*
	 * A fourth string (if provided) in the form <K> may be given, where:
	 *  - K:	wireless ID (in hex)
	 * this information goes into the following places on the fuses:
	 *  - K:	MAC1_ADDR0 bits 31..30 (2 bits)
	 * If not provided, a zero is used (for backwards compatibility)
	 */
	if (argc > 3) {
		if (strlen(argv[3]) != 1)
			goto err;
	}

	/* Year (only 6 bits: from 0 to 63) */
	strncpy(tmp, &argv[0][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 63)
		goto err;
	hwid->year = num;
	printf("    Year:          20%02d\n", hwid->year);

	/* Month */
	strncpy(tmp, &argv[0][2], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 1 || num > 12)
		goto err;
	hwid->month = num;
	printf("    Month:         %02d\n", hwid->month);

	/* Generator ID */
	strncpy(tmp, &argv[0][4], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 15)
		goto err;
	hwid->genid = num;
	printf("    Generator ID:  %02d\n", hwid->genid);

	/* Serial number */
	strncpy(tmp, &argv[0][6], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 999999)
		goto err;
	hwid->sn = num;
	printf("    S/N:           %06d\n", hwid->sn);

	/* MAC pool */
	strncpy(tmp, &argv[1][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 15)
		goto err;
	hwid->mac_pool = num;
	printf("    MAC pool:      %02d\n", hwid->mac_pool);

	/* MAC base address */
	strncpy(tmp, &argv[1][2], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0xFFFFFF)
		goto err;
	hwid->mac_base = num;
	printf("    MAC Base:      %.2x:%.2x:%.2x\n",
		(hwid->mac_base >> 16) & 0xFF,
		(hwid->mac_base >> 8) & 0xFF,
		(hwid->mac_base) & 0xFF);

	/* Variant */
	strncpy(tmp, &argv[2][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0x1F)
		goto err;
	hwid->variant = num;
	printf("    Variant:       0x%02x\n", hwid->variant);

	/* Hardware version */
	strncpy(tmp, &argv[2][2], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 7)
		goto err;
	hwid->hv = num;
	printf("    HW version:    0x%x\n", hwid->hv);

	/* Cert */
	strncpy(tmp, &argv[2][3], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 7)
		goto err;
	hwid->cert = num;
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
	       hwid->cert < ARRAY_SIZE(cert_regions) ?
	       cert_regions[hwid->cert] : "??");

	if (argc > 2) {
		/* Wireless ID */
		strncpy(tmp, &argv[3][0], 1);
		tmp[1] = 0;
		num = simple_strtol(tmp, NULL, 16);
		if (num > 3)
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
	/* SCU performs automatic lock after programming */
	printf("not supported. Fuses automatically locked after programming.\n");
	return 1;
}

void fdt_fixup_hwid(void *fdt)
{
	const char *propnames[] = {
		"digi,hwid,year",
		"digi,hwid,month",
		"digi,hwid,genid",
		"digi,hwid,sn",
		"digi,hwid,macpool",
		"digi,hwid,macbase",
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
		if (!strcmp("digi,hwid,year", propnames[i]))
			sprintf(str, "20%02d", my_hwid.year);
		else if (!strcmp("digi,hwid,month", propnames[i]))
			sprintf(str, "%02d", my_hwid.month);
		else if (!strcmp("digi,hwid,genid", propnames[i]))
			sprintf(str, "%02d", my_hwid.genid);
		else if (!strcmp("digi,hwid,sn", propnames[i]))
			sprintf(str, "%06d", my_hwid.sn);
		else if (!strcmp("digi,hwid,macpool", propnames[i]))
			sprintf(str, "%02d", my_hwid.mac_pool);
		else if (!strcmp("digi,hwid,macbase", propnames[i]))
			sprintf(str, "%06x", my_hwid.mac_base);
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
