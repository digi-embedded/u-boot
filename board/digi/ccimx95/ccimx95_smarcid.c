// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2026, Digi International, Inc.
 */

#include <cli_hush.h>
#include <common.h>
#include <fdt_support.h>
#include <fuse.h>
#include <linux/sizes.h>
#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/smarcid.h"

#define ASSERT_SMARCID_WORD_LAYOUT() \
	assert(sizeof(struct digi_smarcid) == smarcid_nwords * sizeof(u32))

/* SMARCID fuse map */
struct digi_hwid_fuse smarcid_fuse_map[] = {
	/* bank, word, len */
	{40, 5, 8},	/* COM_DEVICE_ID_CFG3[31:0] */
	{40, 6, 8},	/* COM_DEVICE_ID_CFG4[31:0] */
};

unsigned int smarcid_nwords = ARRAY_SIZE(smarcid_fuse_map);

char smarc_locations[3] = {'D','I','R'};

char board_smarcid_get_location_char(int location_index)
{
	if (location_index < 0 || 
	    location_index >= ARRAY_SIZE(smarc_locations))
		return '-';

	return (smarc_locations[location_index]);
}

static int get_location_index(char location_char)
{
	switch (location_char) {
		case 'D':
		return 0;

		case 'I':
		return 1;

		case 'R':
		return 2;

		default:
		return -1;
	}
}

/* Print SMARCID info */
void board_smarcid_print(const struct digi_smarcid *smarcid)
{
	board_smarcid_print_hex(smarcid);

	/* Formatted printout */
	printf("    Variant:       0x%02x\n", smarcid->variant);
	printf("      MCA:         %s\n", smarcid->mca ? "yes" : "-");
	printf("      ETH1:        %s\n", smarcid->eth1 ? "yes" : "-");
	printf("      ETH2:        %s\n", smarcid->eth2 ? "yes" : "-");
	printf("      USB Hub:     %s\n", smarcid->hub ? "yes" : "-");
	printf("      TPM:         %s\n", smarcid->tpm ? "yes" : "-");
	printf("      RTC:         %s\n", smarcid->rtc ? "yes" : "-");
	printf("      Temp sensor: %s\n", smarcid->temp ? "yes" : "-");
	printf("      EEPROM:      %s\n", smarcid->eeprom ? "yes" : "-");
	printf("    HW Version:    0x%x\n", smarcid->hv);
	printf("    Location:      %c (%d)\n",
	       board_smarcid_get_location_char(smarcid->location),
	       smarcid->location);
	printf("    Year:          20%02d\n", smarcid->year);
	printf("    Week:          %02d\n", smarcid->week);
	printf("    S/N:           %06d\n", smarcid->sn);
	

	return;
}

/* Print SMARCID info in MANUFID format */
void board_smarcid_print_manuf(const struct digi_smarcid *smarcid)
{
	board_smarcid_print_hex(smarcid);

	/* Formatted printout */
	printf(" Manufacturing ID: %c%02d%02d%02d%06d %02x%x"
	       " %x%x%x%x%x%x%x%x\n",
		board_smarcid_get_location_char(smarcid->location),
		smarcid->year,
		smarcid->week,
		CONFIG_DIGI_FAMILY_ID,
		smarcid->sn,
		smarcid->variant,
		smarcid->hv,
		smarcid->mca,
		smarcid->eth1,
		smarcid->eth2,
		smarcid->hub,
		smarcid->tpm,
		smarcid->rtc,
		smarcid->temp,
		smarcid->eeprom);
}

static int parse_bool_char(char c, bool *val)
{
	int v = c - '0';

	if (v != 0 && v != 1)
		return -1;

	*val = (bool)v;

	return 0;
}

/* Parse SMARCID info in MANUF format */
int board_smarcid_parse_manuf(int argc, char *const argv[], struct digi_smarcid *smarcid)
{
	char tmp[13];
	unsigned long num;
	bool v;
	int index;

	/* Initialize SMARCID words */
	memset(smarcid, 0, sizeof(struct digi_smarcid));

	if (argc != 3)
		goto smarc_err;

	/*
	 * Digi Manufacturing team produces a string in the form
	 *     <LYYWWFFXXXXXX> <VVH> <MEEUTRSA>
	 */

	/* <LYYWWFFXXXXXX>, where:
	 *  - L:        location code (in decimal)
	 *  - YY:       year (last two digits of XXI century, in decimal)
	 *  - WW:       week of year (in decimal)
	 *  - FF:       family code (in decimal)
	 *  - XXXXXX:   serial number (in decimal)
	 */
	if (strlen(argv[0]) != 13)
		goto smarc_err;

	/*
	 * <VVH>, where:
	 *  - VV:       variant (in hex)
	 *  - H:        hardware version (in hex)
	 */
	if (strlen(argv[1]) != 3)
		goto smarc_err;

	/*
	 * <MEEUTRSA>, where:
	 *  - M:        whether the variant has MCA chip
	 *  - EE:       whether the variant has both, one or no Ethernet PHYs
	 *  - U:        whether the variant has USB Hub chip
	 *  - T:        whether the variant has TPM module
	 *  - R:        whether the variant has RTC module
	 *  - S:        whether the variant has Temperature sensor
	 *  - A:        whether the variant has EEPROM chip
	 */
	if (strlen(argv[2]) != 8)
		goto smarc_err;


	/* Location */
	index = get_location_index(argv[0][0]);
	if (index < 0) {
		printf("Invalid location\n");
		goto smarc_err;
	}
	smarcid->location = index;
	printf("    Location:      %c (%d)\n", board_smarcid_get_location_char(smarcid->location), smarcid->location);

	/* Year (only 6 bits: from 0 to 63) */
	strncpy(tmp, &argv[0][1], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 63) {
		printf("Invalid year (max is 63)\n");
		goto smarc_err;
	}
	smarcid->year = num;
	printf("    Year:          20%02d\n", smarcid->year);

	/* Week */
	strncpy(tmp, &argv[0][3], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num < 1 || num > 54) {
		printf("Invalid week (min is 1, max is 54)\n");
		goto smarc_err;
	}
	smarcid->week = num;
	printf("    Week:          %02d\n", smarcid->week);

	/* Family ID (validate, but not store) */
	strncpy(tmp, &argv[0][5], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num != CONFIG_DIGI_FAMILY_ID) {
		printf("Invalid Family ID (expected %u)\n", CONFIG_DIGI_FAMILY_ID);
		goto smarc_err;
	}
	printf("    Family ID:     %02d\n", CONFIG_DIGI_FAMILY_ID);

	/* Serial number */
	strncpy(tmp, &argv[0][7], 6);
	tmp[6] = 0;
	num = simple_strtol(tmp, NULL, 10);
	if (num > 999999) {
		printf("Invalid serial number (max is 999999)\n");
		goto smarc_err;
	}
	smarcid->sn = num;
	printf("    S/N:           %06d\n", smarcid->sn);

	/* Variant */
	strncpy(tmp, &argv[1][0], 2);
	tmp[2] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0x1F) {
		printf("Invalid variant (max is 1F)\n");
		goto smarc_err;
	}
	smarcid->variant = num;
	printf("    Variant:       0x%02x\n", smarcid->variant);

	/* Hardware version */
	strncpy(tmp, &argv[1][2], 1);
	tmp[1] = 0;
	num = simple_strtol(tmp, NULL, 16);
	if (num > 0xF) {
		printf("Invalid hardware version (max is F)\n");
		goto smarc_err;
	}
	smarcid->hv = num;
	printf("    HW version:    0x%x\n", smarcid->hv);

	/* MCA */
	if (parse_bool_char(argv[2][0], &v)) {
		printf("Invalid MCA\n");
		goto smarc_err;
	}
	smarcid->mca = v;
	printf("    MCA:           %s\n", smarcid->mca ? "yes" : "-");

	/* ETH1 */
	if (parse_bool_char(argv[2][1], &v)) {
		printf("Invalid ETH1\n");
		goto smarc_err;
	}
	smarcid->eth1 = v;
	printf("    ETH1:          %s\n", smarcid->eth1 ? "yes" : "-");

	/* ETH2 */
	if (parse_bool_char(argv[2][2], &v)) {
		printf("Invalid ETH2\n");
		goto smarc_err;
	}
	smarcid->eth2 = v;
	printf("    ETH2:          %s\n", smarcid->eth2 ? "yes" : "-");

	/* USB Hub */
	if (parse_bool_char(argv[2][3], &v)) {
		printf("Invalid USB Hub\n");
		goto smarc_err;
	}
	smarcid->hub = v;
	printf("    USB Hub:       %s\n", smarcid->hub ? "yes" : "-");

	/* TPM module */
	if (parse_bool_char(argv[2][4], &v)) {
		printf("Invalid TPM module\n");
		goto smarc_err;
	}
	smarcid->tpm = v;
	printf("    TPM:           %s\n", smarcid->tpm ? "yes" : "-");

	/* RTC module */
	if (parse_bool_char(argv[2][5], &v)) {
		printf("Invalid RTC module\n");
		goto smarc_err;
	}
	smarcid->rtc = v;
	printf("    RTC:           %s\n", smarcid->rtc ? "yes" : "-");

	/* Temperature sensor */
	if (parse_bool_char(argv[2][6], &v)) {
		printf("Invalid Temp sensor\n");
		goto smarc_err;
	}
	smarcid->temp = v;
	printf("    Temp:          %s\n", smarcid->temp ? "yes" : "-");

	/* EEPROM */
	if (parse_bool_char(argv[2][7], &v)) {
		printf("Invalid EEPROM\n");
		goto smarc_err;
	}
	smarcid->eeprom = v;
	printf("    EEPROM:        %s\n", smarcid->eeprom ? "yes" : "-");

	/* If format was correct, set SMARC MAGIC field */
	smarcid->magic = SMARC_MAGIC;

	return 0;

smarc_err:
	printf("Invalid manufacturing string.\n"
	       "Manufacturing information must be in the form: "
	       CONFIG_SMARCID_MANUF_STRINGS_HELP "\n");

	return -EINVAL;
}

void fdt_fixup_smarcid(void *fdt, const struct digi_smarcid *smarcid)
{
	const char *propnames[] = {
		"digi,smarcid,location",
		"digi,smarcid,year",
		"digi,smarcid,week",
		"digi,smarcid,sn",
		"digi,smarcid,variant",
		"digi,smarcid,hv",
		"digi,smarcid,has-mca",
		"digi,smarcid,has-eth1",
		"digi,smarcid,has-eth2",
		"digi,smarcid,has-usb-hub",
		"digi,smarcid,has-tpm",
		"digi,smarcid,has-rtc",
		"digi,smarcid,has-temp",
		"digi,smarcid,has-eeprom",
	};
	char str[20];

	ASSERT_SMARCID_WORD_LAYOUT();

	/* Skip function for SMT modules */
	if (!is_smarc(smarcid))
		return;

	/* Register the SMARCID as main node properties in the FDT */
	for (int i = 0; i < ARRAY_SIZE(propnames); i++) {

		/* Convert SMARCID fields to strings */
		if (!strcmp("digi,smarcid,location", propnames[i]))
			sprintf(str, "%c", board_smarcid_get_location_char(smarcid->location));
		else if (!strcmp("digi,smarcid,year", propnames[i]))
			sprintf(str, "20%02d", smarcid->year);
		else if (!strcmp("digi,smarcid,week", propnames[i]))
			sprintf(str, "%02d", smarcid->week);
		else if (!strcmp("digi,smarcid,sn", propnames[i]))
			sprintf(str, "%06d", smarcid->sn);
		else if (!strcmp("digi,smarcid,variant", propnames[i]))
			sprintf(str, "0x%02x", smarcid->variant);
		else if (!strcmp("digi,smarcid,hv", propnames[i]))
			sprintf(str, "0x%x", smarcid->hv);
		else if ((!strcmp("digi,smarcid,has-mca", propnames[i]) &&
			  smarcid->mca) ||
			 (!strcmp("digi,smarcid,has-eth1", propnames[i]) &&
			  smarcid->eth1) ||
			 (!strcmp("digi,smarcid,has-eth2", propnames[i]) &&
			  smarcid->eth2) ||
			 (!strcmp("digi,smarcid,has-usb-hub", propnames[i]) &&
			  smarcid->hub) ||
			 (!strcmp("digi,smarcid,has-tpm", propnames[i]) &&
			  smarcid->tpm) ||
			 (!strcmp("digi,smarcid,has-rtc", propnames[i]) &&
			  smarcid->rtc) ||
			 (!strcmp("digi,smarcid,has-temp", propnames[i]) &&
			  smarcid->temp) ||
			 (!strcmp("digi,smarcid,has-eeprom", propnames[i]) &&
			  smarcid->eeprom))
			strcpy(str, "");
		else
			continue;

		do_fixup_by_path(fdt, "/", propnames[i], str,
				 strlen(str) + 1, 1);
	}

	/* Register SMARCID words in the device tree */
	for (int i = 0; i < smarcid_nwords; i++) {
		sprintf(str, "digi,smarcid_%d", i);
		do_fixup_by_path_u32(fdt, "/", str, *((u32 *)smarcid + i), 1);
	}
}
