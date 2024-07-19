/*
 * Copyright (C) 2024 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdt_support.h>
#include <fuse.h>
#include <linux/sizes.h>
#include "../common/helper.h"
#include "../common/hwid.h"

const char *cert_regions[] = {
	"U.S.A.",
	"International",
	"Japan",
};

int hwid_word_lengths[CONFIG_HWID_WORDS_NUMBER] = {8, 8, 8};

u32 ram_sizes_mb[16] = {
	0,	/* 0 */
	16,	/* 1 */
	32,	/* 2 */
	64,	/* 3 */
	128,	/* 4 */
	256,	/* 5 */
	512,	/* 6 */
	1024,	/* 7 */
	2048,	/* 8 */
	3072,	/* 9 */
	4096,	/* A */
	/* yet undefined */
	0,	/* B */
	0,	/* C */
	0,	/* D */
	0,	/* E */
	0,	/* F */
};

/* Print HWID info */
void board_print_hwid(struct digi_hwid *hwid)
{
	print_hwid_hex(hwid);

	/* Formatted printout */
	printf("    Generator ID:  %02d\n", hwid->genid);
	printf("    MAC Pool:      %02d\n", hwid->mac_pool);
	printf("    MAC Base:      %.2x:%.2x:%.2x\n",
		(hwid->mac_base >> 16) & 0xFF,
		(hwid->mac_base >> 8) & 0xFF,
		(hwid->mac_base) & 0xFF);
	printf("    Variant:       0x%02x\n", hwid->variant);
	/* New fields (supported if 'RAM' field != 0) */
	if (hwid->ram) {
		printf("      RAM:         %u MiB\n", ram_sizes_mb[hwid->ram]);
		printf("      Wi-Fi:       %s\n", hwid->wifi ? "yes" : "-");
		printf("      Bluetooth:   %s\n", hwid->bt ? "yes" : "-");
	}
	printf("    HW Version:    0x%x\n", hwid->hv);
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
		hwid->cert < ARRAY_SIZE(cert_regions) ?
		cert_regions[hwid->cert] : "??");
	printf("    Year:          20%02d\n", hwid->year);
	printf("    Week:          %02d\n", hwid->week);
	printf("    S/N:           %06d\n", hwid->sn);
}

/* Print HWID info in MANUFID format */
void board_print_manufid(struct digi_hwid *hwid)
{
	print_hwid_hex(hwid);

	/* Formatted printout */
	printf(" Manufacturing ID: %02d%02d%02d%06d %02d%06x %02x%x%x"
	       " %x%x%x\n",
		hwid->year,
		hwid->week,
		hwid->genid,
		hwid->sn,
		hwid->mac_pool,
		hwid->mac_base,
		hwid->variant,
		hwid->hv,
		hwid->cert,
		hwid->ram,
		hwid->wifi,
		hwid->bt);
}

/* Parse HWID info in HWID format */
int board_parse_hwid(int argc, char *const argv[], struct digi_hwid *hwid)
{
	int i, word;
	u32 hwidword;

	if (argc != CONFIG_HWID_WORDS_NUMBER)
		goto err;

	/* Parse backwards, from MSB to LSB */
	word = CONFIG_HWID_WORDS_NUMBER - 1;
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word--)
		if (strlen(argv[i]) > hwid_word_lengths[word])
			goto err;

	/*
	 * Digi HWID is set as a number of hex strings in the form
	 *   CCMP2: <XXXXXXXX> <YYYYYYYY> <ZZZZZZZZ>
	 * that are inversely stored into the structure.
	 */

	/* Parse backwards, from MSB to LSB */
	word = CONFIG_HWID_WORDS_NUMBER - 1;
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++, word--) {
		if (strtou32(argv[i], 16, &hwidword))
			goto err;

		((u32 *)hwid)[word] = hwidword;
	}
	board_print_hwid(hwid);

	return 0;

err:
	printf("Invalid HWID input.\n"
		"HWID input must be in the form: "
		CONFIG_HWID_STRINGS_HELP "\n");
	return -EINVAL;
}

static int parse_bool_char(char c, bool *val)
{
	int v = c -'0';

	if (v != 0 && v != 1)
		return -1;

	*val = (bool)v;

	return 0;
}

/* Parse HWID info in MANUFID format */
int board_parse_manufid(int argc, char *const argv[], struct digi_hwid *hwid)
{
	char tmp[13];
	unsigned long num;

	/* Initialize HWID words */
	memset(hwid, 0, sizeof(struct digi_hwid));

	if (argc != 4)
		goto err;

	/*
	 * Digi Manufacturing team produces a string in the form
	 *     <YYWWGGXXXXXX> <PPAAAAAA> <VVHC> <RMWBC>
	 */

	/* <YYWWGGXXXXXX>, where:
	 *  - YY:	year (last two digits of XXI century, in decimal)
	 *  - WW:	week of year (in decimal)
	 *  - GG:	generator ID (in decimal)
	 *  - XXXXXX:	serial number (in decimal)
	 */
	if (strlen(argv[0]) != 12)
		goto err;

	/*
	 * <PPAAAAAA>, where:
	 *  - PP:	MAC pool (in decimal)
	 *  - AAAAAA:	MAC base address (in hex)
	 */
	if (strlen(argv[1]) != 8)
		goto err;

	/*
	 * <VVHC>, where:
	 *  - VV:	variant (in hex)
	 *  - H:	hardware version (in hex)
	 *  - C:	wireless certification (in hex)
	 */
	if (strlen(argv[2]) != 4)
		goto err;

	/*
	 * <RWB>, where:
	 *  - R:	ram (index to ram_sizes_mb[] array)
	 *  - W:	whether the variant has Wi-Fi chip
	 *  - B:	whether the variant has Bluetooth chip
	 */
	if (strlen(argv[3]) != 3)
		goto err;

	/* Year (only 6 bits: from 0 to 63) */
	strncpy(tmp, &argv[0][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 63) {
		printf("Invalid year\n");
		goto err;
	}
	hwid->year = num;
	printf("    Year:          20%02d\n", hwid->year);

	/* Week */
	strncpy(tmp, &argv[0][2], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 1 || num > 54) {
		printf("Invalid week\n");
		goto err;
	}
	hwid->week = num;
	printf("    Week:          %02d\n", hwid->week);

	/* Generator ID */
	strncpy(tmp, &argv[0][4], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 15) {
		printf("Invalid Generator ID\n");
		goto err;
	}
	hwid->genid = num;
	printf("    Generator ID:  %02d\n", hwid->genid);

	/* Serial number */
	strncpy(tmp, &argv[0][6], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 999999) {
		printf("Invalid serial number\n");
		goto err;
	}
	hwid->sn = num;
	printf("    S/N:           %06d\n", hwid->sn);

	/* MAC pool */
	strncpy(tmp, &argv[1][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 15) {
		printf("Invalid MAC pool\n");
		goto err;
	}
	hwid->mac_pool = num;
	printf("    MAC pool:      %02d\n", hwid->mac_pool);

	/* MAC base address */
	strncpy(tmp, &argv[1][2], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0xFFFFFF) {
		printf("Invalid MAC base address");
		goto err;
	}
	hwid->mac_base = num;
	printf("    MAC Base:      %.2x:%.2x:%.2x\n",
		(hwid->mac_base >> 16) & 0xFF,
		(hwid->mac_base >> 8) & 0xFF,
		(hwid->mac_base) & 0xFF);

	/* Variant */
	strncpy(tmp, &argv[2][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0x1F) {
		printf("Invalid variant\n");
		goto err;
	}
	hwid->variant = num;
	printf("    Variant:       0x%02x\n", hwid->variant);

	/* Hardware version */
	strncpy(tmp, &argv[2][2], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 7) {
		printf("Invalid hardware version\n");
		goto err;
	}
	hwid->hv = num;
	printf("    HW version:    0x%x\n", hwid->hv);

	/* Cert */
	strncpy(tmp, &argv[2][3], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 7) {
		printf("Invalid cert\n");
		goto err;
	}
	hwid->cert = num;
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
	       hwid->cert < ARRAY_SIZE(cert_regions) ?
	       cert_regions[hwid->cert] : "??");

	if (argc > 3) {
		bool v;

		/* RAM */
		strncpy(tmp, &argv[3][0], 1);
		tmp[1] = 0;
		num = simple_strtol(tmp, NULL, 16);
		if (num < 1) {
			printf("Invalid RAM\n");
			goto err;
		}
		hwid->ram = num;
		/* Wi-Fi */
		if (parse_bool_char(argv[3][1], &v)) {
			printf("Invalid Wi-Fi\n");
			goto err;
		}
		hwid->wifi = v;
		/* Bluetooth */
		if (parse_bool_char(argv[3][2], &v)) {
			printf("Invalid Bluetooth\n");
			goto err;
		}
		hwid->bt = v;
	}
	printf("    RAM:           %u MiB\n", ram_sizes_mb[hwid->ram]);
	printf("    Wi-Fi:         %s\n", hwid->wifi ? "yes" : "-");
	printf("    Bluetooth:     %s\n", hwid->bt ? "yes" : "-");

	return 0;

err:
	printf("Invalid manufacturing string.\n"
		"Manufacturing information must be in the form: "
		CONFIG_MANUF_STRINGS_HELP "\n");
	return -EINVAL;
}

void fdt_fixup_hwid(void *fdt, const struct digi_hwid *hwid)
{
	const char *propnames[] = {
		"digi,hwid,year",
		"digi,hwid,week",
		"digi,hwid,genid",
		"digi,hwid,sn",
		"digi,hwid,macpool",
		"digi,hwid,macbase",
		"digi,hwid,variant",
		"digi,hwid,hv",
		"digi,hwid,cert",
		"digi,hwid,ram_mb",
		"digi,hwid,has-wifi",
		"digi,hwid,has-bt",
	};
	char str[20];
	int i;
	/* Capabilities fields available if RAM != 0 */
	bool capabilities = !!hwid->ram;

	/* Register the HWID as main node properties in the FDT */
	for (i = 0; i < ARRAY_SIZE(propnames); i++) {

		/* Convert HWID fields to strings */
		if (!strcmp("digi,hwid,year", propnames[i]))
			sprintf(str, "20%02d", hwid->year);
		else if (!strcmp("digi,hwid,week", propnames[i]))
			sprintf(str, "%02d", hwid->week);
		else if (!strcmp("digi,hwid,genid", propnames[i]))
			sprintf(str, "%02d", hwid->genid);
		else if (!strcmp("digi,hwid,sn", propnames[i]))
			sprintf(str, "%06d", hwid->sn);
		else if (!strcmp("digi,hwid,macpool", propnames[i]))
			sprintf(str, "%02d", hwid->mac_pool);
		else if (!strcmp("digi,hwid,macbase", propnames[i]))
			sprintf(str, "%06x", hwid->mac_base);
		else if (!strcmp("digi,hwid,variant", propnames[i]))
			sprintf(str, "0x%02x", hwid->variant);
		else if (!strcmp("digi,hwid,hv", propnames[i]))
			sprintf(str, "0x%x", hwid->hv);
		else if (!strcmp("digi,hwid,cert", propnames[i]))
			sprintf(str, "0x%x", hwid->cert);
		/* capabilties fields */
		else if (capabilities &&
			 !strcmp("digi,hwid,ram_mb", propnames[i]))
			sprintf(str, "%u", ram_sizes_mb[hwid->ram]);
		else if (capabilities &&
			 ((!strcmp("digi,hwid,has-wifi", propnames[i]) &&
			   hwid->wifi) ||
			   (!strcmp("digi,hwid,has-bt", propnames[i]) &&
			   hwid->bt)))
			strcpy(str, "");
		else
			continue;

		do_fixup_by_path(fdt, "/", propnames[i], str,
				strlen(str) + 1, 1);
	}

	/* Register HWID words in the device tree */
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		sprintf(str, "digi,hwid_%d", i);
		do_fixup_by_path_u32(fdt, "/", str, *((u32 *)hwid + i), 1);
	}
}

u32 hwid_get_ramsize(const struct digi_hwid *hwid)
{
	return ram_sizes_mb[hwid->ram] * SZ_1M;
}

int board_lock_hwid(void)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_lock(bank, word);
		if (ret)
			return ret;
	}

	return 0;
}
