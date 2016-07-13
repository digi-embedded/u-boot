/*
 * Copyright (C) 2016 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <fuse.h>
#include <asm/errno.h>
#include "helper.h"
#include "hwid.h"
#include <fdt_support.h>
#include "../ccimx6/ccimx6.h"

const char *cert_regions[] = {
	"U.S.A.",
	"International",
	"Japan",
};

void ccimx6_print_hwid(u32 *buf)
{
	int i;
	int cert;

	for (i = CONFIG_HWID_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", buf[i]);
	printf("\n");
	/* Formatted printout */
	printf("    Year:          20%02d\n", (buf[1] >> 26) & 0x3f);
	printf("    Week:          %02d\n", (buf[1] >> 20) & 0x3f);
	printf("    Variant:       0x%02x\n", (buf[1] >> 8) & 0xff);
	printf("    HW Version:    0x%x\n", (buf[1] >> 4) & 0xf);
	cert = buf[1] & 0xf;
	printf("    Cert:          0x%x (%s)\n", cert,
	       cert < ARRAY_SIZE(cert_regions) ? cert_regions[cert] : "??");
	printf("    Location:      %c\n", ((buf[0] >> 27) & 0x1f) + 'A');
	printf("    Generator ID:  %02d\n", (buf[0] >> 20) & 0x7f);
	printf("    S/N:           %06d\n", buf[0] & 0xfffff);
}

void ccimx6_print_manufid(u32 *buf)
{
	int i;

	for (i = CONFIG_HWID_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", buf[i]);
	printf("\n");
	/* Formatted printout */
	printf(" Manufacturing ID: %c%02d%02d%02d%06d %02x%x%x\n",
	       ((buf[0] >> 27) & 0x1f) + 'A',
	       (buf[1] >> 26) & 0x3f,
	       (buf[1] >> 20) & 0x3f,
	       (buf[0] >> 20) & 0x7f,
	       buf[0] & 0xfffff,
	       (buf[1] >> 8) & 0xff,
	       (buf[1] >> 4) & 0xf,
	       buf[1] & 0xf);
}

static void array_to_hwid(u32 *buf, struct ccimx6_hwid *hwid)
{
	/*
	 *                      MAC1 (Bank 4 Word 3)
	 *
	 *       | 31..26 | 25..20 |   |  15..8  | 7..4 | 3..0 |
	 *       +--------+--------+---+---------+------+------+
	 * HWID: |  Year  |  Week  | - | Variant |  HV  | Cert |
	 *       +--------+--------+---+---------+------+------+
	 *
	 *                      MAC0 (Bank 4 Word 2)
	 *
	 *       |  31..27  | 26..20 |         19..0           |
	 *       +----------+--------+-------------------------+
	 * HWID: | Location |  GenID |      Serial number      |
	 *       +----------+--------+-------------------------+
	 */
	hwid->year = (buf[1] >> 26) & 0x3f;
	hwid->week = (buf[1] >> 20) & 0x3f;
	hwid->variant = (buf[1] >> 8) & 0xff;
	hwid->hv = (buf[1] >> 4) & 0xf;
	hwid->cert = buf[1] & 0xf;
	hwid->location = (buf[0] >> 27) & 0x1f;
	hwid->genid = (buf[0] >> 20) & 0x7f;
	hwid->sn = buf[0] & 0xfffff;
}

int ccimx6_manufstr_to_hwid(int argc, char *const argv[], u32 *val)
{
	u32 *mac0 = val;
	u32 *mac1 = val + 1;
	char tmp[13];
	unsigned long num;

	/* Initialize HWID words */
	*mac0 = 0;
	*mac1 = 0;

	if (argc != 2)
		goto err;

	/*
	 * Digi Manufacturing team produces a string in the form
	 *     LYYWWGGXXXXXX
	 * where:
	 *  - L:	location, an uppercase letter [A..Z]
	 *  - YY:	year (last two digits of XXI century, in decimal)
	 *  - WW:	week of year (in decimal)
	 *  - GG:	generator ID (in decimal)
	 *  - XXXXXX:	serial number (in decimal)
	 * this information goes into the following places on the HWID:
	 *  - L:	OCOTP_MAC0 bits 31..27 (5 bits)
	 *  - YY:	OCOTP_MAC1 bits 31..26 (6 bits)
	 *  - WW:	OCOTP_MAC1 bits 25..20 (6 bits)
	 *  - GG:	OCOTP_MAC0 bits 26..20 (7 bits)
	 *  - XXXXXX:	OCOTP_MAC0 bits 19..0 (20 bits)
	 */
	if (strlen(argv[0]) != 13)
		goto err;

	/*
	 * Additionally a second string in the form VVHC must be given where:
	 *  - VV:	variant (in hex)
	 *  - H:	hardware version (in hex)
	 *  - C:	wireless certification (in hex)
	 * this information goes into the following places on the HWID:
	 *  - VV:	OCOTP_MAC1 bits 15..8 (8 bits)
	 *  - H:	OCOTP_MAC1 bits 7..4 (4 bits)
	 *  - C:	OCOTP_MAC1 bits 3..0 (4 bits)
	 */
	if (strlen(argv[1]) != 4)
		goto err;

	/* Location */
	if (argv[0][0] < 'A' || argv[0][0] > 'Z')
		goto err;
	*mac0 |= (argv[0][0] - 'A') << 27;
	printf("    Location:      %c\n", argv[0][0]);

	/* Year (only 6 bits: from 0 to 63) */
	strncpy(tmp, &argv[0][1], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 0 || num > 63)
		goto err;
	*mac1 |= num << 26;
	printf("    Year:          20%02d\n", (int)num);

	/* Week */
	strncpy(tmp, &argv[0][3], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 1 || num > 54)
		goto err;
	*mac1 |= num << 20;
	printf("    Week:          %02d\n", (int)num);

	/* Generator ID */
	strncpy(tmp, &argv[0][5], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 0 || num > 99)
		goto err;
	*mac0 |= num << 20;
	printf("    Generator ID:  %02d\n", (int)num);

	/* Serial number */
	strncpy(tmp, &argv[0][7], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 0 || num > 999999)
		goto err;
	*mac0 |= num;
	printf("    S/N:           %06d\n", (int)num);

	/* Variant */
	strncpy(tmp, &argv[1][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num < 0 || num > 0xff)
		goto err;
	*mac1 |= num << 8;
	printf("    Variant:       0x%02x\n", (int)num);

	/* Hardware version */
	strncpy(tmp, &argv[1][2], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num < 0 || num > 0xf)
		goto err;
	*mac1 |= num << 4;
	printf("    HW version:    0x%x\n", (int)num);

	/* Cert */
	strncpy(tmp, &argv[1][3], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num < 0 || num > 0xf)
		goto err;
	*mac1 |= num;
	printf("    Cert:          0x%x (%s)\n", (int)num,
	       num < ARRAY_SIZE(cert_regions) ? cert_regions[num] : "??");

	return 0;

err:
	printf("Invalid manufacturing string.\n"
		"Manufacturing information must be in the form: "
		CONFIG_MANUF_STRINGS_HELP "\n");
	return -EINVAL;
}

int ccimx6_get_hwid(struct ccimx6_hwid *hwid)
{
	u32 buf[CONFIG_HWID_WORDS_NUMBER];
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_read(bank, word, &buf[i]);
		if (ret)
			return -1;
	}

	array_to_hwid(buf, hwid);

	return 0;
}

void ccimx6_fdt_fixup_hwid(void *fdt, struct ccimx6_hwid *hwid)
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
	};
	char str[20];
	int i;

	/* Register the HWID as main node properties in the FDT */
	for (i = 0; i < ARRAY_SIZE(propnames); i++) {
		/* Convert HWID fields to strings */
		if (!strcmp("digi,hwid,location", propnames[i]))
			sprintf(str, "%c", hwid->location + 'A');
		else if (!strcmp("digi,hwid,genid", propnames[i]))
			sprintf(str, "%02d", hwid->genid);
		else if (!strcmp("digi,hwid,sn", propnames[i]))
			sprintf(str, "%06d", hwid->sn);
		else if (!strcmp("digi,hwid,year", propnames[i]))
			sprintf(str, "20%02d", hwid->year);
		else if (!strcmp("digi,hwid,week", propnames[i]))
			sprintf(str, "%02d", hwid->week);
		else if (!strcmp("digi,hwid,variant", propnames[i]))
			sprintf(str, "0x%02x", hwid->variant);
		else if (!strcmp("digi,hwid,hv", propnames[i]))
			sprintf(str, "0x%x", hwid->hv);
		else if (!strcmp("digi,hwid,cert", propnames[i]))
			sprintf(str, "0x%x", hwid->cert);
		else
			continue;

		do_fixup_by_path(fdt, "/", propnames[i], str,
				 strlen(str) + 1, 1);
	}
}
