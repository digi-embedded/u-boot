/*
 * Copyright (C) 2018-2026 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <env.h>
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

/* HWID fuse map */
struct digi_hwid_fuse hwid_fuse_map[] = {
	/* bank, word, len */
#if CONFIG_IS_ENABLED(CC8X)
	{0, 708, 8},	/* MAC1[31:0] */
	{0, 709, 4},	/* MAC1[47:32] */
	{0, 710, 8},	/* MAC2[31:0] */
	{0, 711, 4},	/* MAC2[47:32] */
#else
	{9, 0, 8},	/* MAC_ADDR0[31..0] */
	{9, 1, 8},	/* MAC_ADDR1[31..0] */
	{9, 2, 8},	/* MAC_ADDR2[31..0] */
#endif /* CC8X or CC8M */
};

unsigned int hwid_nwords = ARRAY_SIZE(hwid_fuse_map);

u64 ram_sizes_mb[16] = {
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

#if CONFIG_IS_ENABLED(CC8X)
void board_hwid_fuse_prog_unlock(void)
{
	env_set("force_prog_ecc", "yes");
}

void board_hwid_fuse_prog_lock(void)
{
	env_set("force_prog_ecc", NULL);
}
#endif /* CC8X */

/* Print HWID info */
void board_hwid_print(struct digi_hwid *hwid)
{
	board_hwid_print_hex(hwid);

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
		printf("      RAM:         %llu MiB\n", ram_sizes_mb[hwid->ram]);
		printf("      Wi-Fi:       %s\n", hwid->wifi ? "yes" : "-");
		printf("      Bluetooth:   %s\n", hwid->bt ? "yes" : "-");
		printf("      Crypto-chip: %s\n", hwid->crypto ? "yes" : "-");
		printf("      MCA:         %s\n", hwid->mca ? "yes" : "-");
	}
	printf("    HW Version:    0x%x\n", hwid->hv);
	printf("    Cert:          0x%x (%s)\n", hwid->cert,
		hwid->cert < ARRAY_SIZE(cert_regions) ?
		cert_regions[hwid->cert] : "??");
	printf("    Wireless ID:   0x%x\n", hwid->wid);
	printf("    Year:          20%02d\n", hwid->year);
	/* If the week is not defined, print the month */
	if (hwid->week)
		printf("    Week:          %02d\n", hwid->week);
	else
		printf("    Month:         %02d\n", hwid->month);
	printf("    S/N:           %06d\n", hwid->sn);
}

/* Print HWID info in MANUFID format */
void board_hwid_print_manuf(struct digi_hwid *hwid)
{
	int week_month;

	board_hwid_print_hex(hwid);

	/* If the week is not defined, print the month */
	if (hwid->week)
		week_month = hwid->week;
	else
		week_month = hwid->month;

	/* Formatted printout */
	printf(" Manufacturing ID: %02d%02d%02d%06d %02d%06x %02x%x%x %x"
	       " %x%x%x%x%x\n",
		hwid->year,
		week_month,
		hwid->genid,
		hwid->sn,
		hwid->mac_pool,
		hwid->mac_base,
		hwid->variant,
		hwid->hv,
		hwid->cert,
		hwid->wid,
		hwid->ram,
		hwid->mca,
		hwid->wifi,
		hwid->bt,
		hwid->crypto);
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
int board_hwid_parse_manuf(int argc, char *const argv[], struct digi_hwid *hwid)
{
	char tmp[13];
	unsigned long num;

	/* Initialize HWID words */
	memset(hwid, 0, sizeof(struct digi_hwid));

	if (argc != 5)
		goto err;

	/*
	 * Digi Manufacturing team produces a string in the form
	 *     <YYWWGGXXXXXX> <PPAAAAAA> <VVHC> <K> <RMWBC>
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
	 * <K>, where:
	 *  - K:	wireless ID (in hex)
	 */
	if (argc > 3) {
		if (strlen(argv[3]) != 1)
			goto err;
	}

	/*
	 * <RMWBC>, where:
	 *  - R:	ram (index to ram_sizes_mb[] array)
	 *  - M:	whether the variant has MCA chip
	 *  - W:	whether the variant has Wi-Fi chip
	 *  - B:	whether the variant has Bluetooth chip
	 *  - C:	whether the variant has crypto-auth chip
	 */
	if (argc > 4) {
		if (strlen(argv[4]) != 5)
			goto err;
	}

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
		/* Wireless ID */
		strncpy(tmp, &argv[3][0], 1);
		tmp[1] = 0;
		num = simple_strtol(tmp, NULL, 16);
		if (num > 3) {
			printf("Invalid Wireless ID\n");
			goto err;
		}
		hwid->wid = num;
	}
	printf("    Wireless ID:   0x%x\n", hwid->wid);

	if (argc > 4) {
		bool v;

		/* RAM */
		strncpy(tmp, &argv[4][0], 1);
		tmp[1] = 0;
		num = simple_strtol(tmp, NULL, 16);
		if (num < 1) {
			printf("Invalid RAM\n");
			goto err;
		}
		hwid->ram = num;
		/* MCA */
		if (parse_bool_char(argv[4][1], &v)) {
			printf("Invalid MCA\n");
			goto err;
		}
		hwid->mca = v;
		/* Wi-Fi */
		if (parse_bool_char(argv[4][2], &v)) {
			printf("Invalid Wi-Fi\n");
			goto err;
		}
		hwid->wifi = v;
		/* Bluetooth */
		if (parse_bool_char(argv[4][3], &v)) {
			printf("Invalid Bluetooth\n");
			goto err;
		}
		hwid->bt = v;
		/* Crypto-chip */
		if (parse_bool_char(argv[4][4], &v)) {
			printf("Invalid Crypto-chip\n");
			goto err;
		}
		hwid->crypto = v;
	}
	printf("    RAM:           %llu MiB\n", ram_sizes_mb[hwid->ram]);
	printf("    Wi-Fi:         %s\n", hwid->wifi ? "yes" : "-");
	printf("    Bluetooth:     %s\n", hwid->bt ? "yes" : "-");
	printf("    Crypto-chip:   %s\n", hwid->crypto ? "yes" : "-");
	printf("    MCA:           %s\n", hwid->mca ? "yes" : "-");

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
		"digi,hwid,month",
		"digi,hwid,week",
		"digi,hwid,genid",
		"digi,hwid,sn",
		"digi,hwid,macpool",
		"digi,hwid,macbase",
		"digi,hwid,variant",
		"digi,hwid,hv",
		"digi,hwid,cert",
		"digi,hwid,wid",
		"digi,hwid,ram_mb",
		"digi,hwid,has-mca",
		"digi,hwid,has-wifi",
		"digi,hwid,has-bt",
		"digi,hwid,has-crypto",
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
		else if (!strcmp("digi,hwid,month", propnames[i]))
			sprintf(str, "%02d", hwid->month);
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
		else if (!strcmp("digi,hwid,wid", propnames[i]))
			sprintf(str, "0x%x", hwid->wid);
		/* capabilties fields */
		else if (capabilities &&
			 !strcmp("digi,hwid,ram_mb", propnames[i]))
			sprintf(str, "%llu", hwid_get_ramsize(hwid)/SZ_1M);
		else if (capabilities &&
			 (((!strcmp("digi,hwid,has-mca", propnames[i]) &&
			   hwid->mca) ||
			   (!strcmp("digi,hwid,has-wifi", propnames[i]) &&
			   hwid->wifi) ||
			   (!strcmp("digi,hwid,has-bt", propnames[i]) &&
			   hwid->bt) ||
			   (!strcmp("digi,hwid,has-crypto", propnames[i]) &&
			   hwid->crypto))))
			strcpy(str, "");
		else
			continue;

		do_fixup_by_path(fdt, "/", propnames[i], str,
				strlen(str) + 1, 1);
	}

	/* Register HWID words in the device tree */
	for (i = 0; i < hwid_nwords; i++) {
		sprintf(str, "digi,hwid_%d", i);
		do_fixup_by_path_u32(fdt, "/", str, *((u32 *)hwid + i), 1);
	}
}

u64 hwid_get_ramsize(const struct digi_hwid *hwid)
{
#ifdef CONFIG_CC8X
	/*
	 * A batch of variant -13 modules was wrongly programmed
	 * with 1GB RAM size. Correct that particular case by
	 * establishing a 4GB RAM size.
	 */
	if ((hwid->variant == 13) && (ram_sizes_mb[hwid->ram] == 1024))
		return SZ_4G;
#endif
	return ram_sizes_mb[hwid->ram] * SZ_1M;
}
