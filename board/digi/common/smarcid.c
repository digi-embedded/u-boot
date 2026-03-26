// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2026, Digi International, Inc.
 */

#include <cli_hush.h>
#include <command.h>
#include <common.h>
#include <linux/errno.h>
#include <fdt_support.h>
#include <fuse.h>
#include <log.h>

#include "helper.h"
#include "hwid.h"
#include "smarcid.h"

extern struct digi_hwid_fuse smarcid_fuse_map[];
extern unsigned int smarcid_nwords;

#define ASSERT_SMARCID_WORD_LAYOUT() \
	assert(sizeof(struct digi_smarcid) == smarcid_nwords * sizeof(u32))

bool is_smarc(const struct digi_smarcid *smarcid)
{
	/* 
	 * Consider device is NOT a SMARC if SMARCID structure does
	 * not have a valid MAGIC
	 */
	return (smarcid->magic == SMARC_MAGIC);
}


int smarcid_read(struct digi_smarcid *smarcid)
{
	int ret;

	/*
	 * If there is a SMARCID defined in the environment, read
	 * it from there. Otherwise, read it from fuses.
	 */
	ret = smarcid_env_read(smarcid);
	if (!ret)
		return ret;

	ret = board_smarcid_fuse_read(smarcid);

	return ret;
}

__weak int board_smarcid_fuse_read(struct digi_smarcid *smarcid)
{
	u32 fuseword;
	int ret;

	ASSERT_SMARCID_WORD_LAYOUT();

	for (int i = 0; i < smarcid_nwords; i++) {
		ret = fuse_sense(smarcid_fuse_map[i].bank,
				 smarcid_fuse_map[i].word,
				 &fuseword);
		if (ret)
			return ret;

		((u32 *)smarcid)[i] = fuseword;
	}

	return 0;
}

__weak int board_smarcid_fuse_set_local_vars(void)
{
	u32 fuseword;
	int ret;
	char var[20];

	u_boot_hush_start();

	for (int i = 0; i < smarcid_nwords; i++) {
		ret = fuse_sense(smarcid_fuse_map[i].bank,
				 smarcid_fuse_map[i].word,
				 &fuseword);
		if (ret)
			return ret;

		/* Set local 'smarcid_n' variables */
		sprintf(var, "smarcid_%d=%08x", i, fuseword);
		set_local_var(var, 0);
	}

	return 0;
}

__weak void board_smarcid_update(void)
{
	/* Do nothing */
}

__weak int board_smarcid_fuse_prog(const struct digi_smarcid *smarcid)
{
	u32 fuseword;
	int ret;

	ASSERT_SMARCID_WORD_LAYOUT();

	for (int i = 0; i < smarcid_nwords; i++) {
		fuseword = ((u32 *)smarcid)[i];
		ret = fuse_prog(smarcid_fuse_map[i].bank,
				smarcid_fuse_map[i].word,
				fuseword);
		if (ret)
			return ret;
	}

	/* Trigger a SMARCID-related variables update */
	board_smarcid_update();

	return 0;
}

__weak int board_smarcid_fuse_lock(void)
{
	return fuse_prog(SMARCID_OCOTP_LOCK_BANK, SMARCID_OCOTP_LOCK_WORD,
			 SMARCID_OCOTP_LOCK_FUSE);
}

__weak void board_smarcid_print_hex(const struct digi_smarcid *smarcid)
{
	ASSERT_SMARCID_WORD_LAYOUT();

	for (int i = smarcid_nwords - 1; i >= 0; i--)
		printf(" %.*x", smarcid_fuse_map[i].len, ((u32 *)smarcid)[i]);

	printf("\n");
}

/* Parse SMARCID info in hex format */
__weak int board_smarcid_parse(int argc, char *const argv[], struct digi_smarcid *smarcid)
{
	int word;
	u32 fuseword;

	ASSERT_SMARCID_WORD_LAYOUT();

	if (argc != smarcid_nwords)
		goto smarc_err;

	/* Parse backwards, from MSB to LSB */
	word = smarcid_nwords - 1;
	for (int i = 0; i < smarcid_nwords; i++, word--)
		if (strlen(argv[i]) > smarcid_fuse_map[word].len)
			goto smarc_err;

	/*
	 * Digi SMARCID is set as a number of hex strings in the form
	 *   CC95: <XXXXXXXX> <YYYYYYYY>
	 * that are inversely stored into the structure.
	 */

	/* Parse backwards, from MSB to LSB */
	word = smarcid_nwords - 1;
	for (int i = 0; i < smarcid_nwords; i++, word--) {
		if (strtou32(argv[i], 16, &fuseword))
			goto smarc_err;

		((u32 *)smarcid)[word] = fuseword;
	}

	/* Verify the given structure is a valid smarcid */
	if (!is_smarc(smarcid))
		goto smarc_err;

	board_smarcid_print(smarcid);

	return 0;

smarc_err:
	printf("Invalid SMARCID input.\n"
		"SMARCID input must be in the form: "
		CONFIG_SMARCID_STRINGS_HELP "\n");
	return -EINVAL;
}

int smarcid_env_read(struct digi_smarcid *smarcid)
{
	char var[20];

	ASSERT_SMARCID_WORD_LAYOUT();

	/* Get SMARCID from 'smarcid_n' variables */
	for (int i = 0; i < smarcid_nwords; i++) {
		sprintf(var, "smarcid_%d", i);
		if (env_get(var) == NULL)
			return -1;

		((u32 *)smarcid)[i] = env_get_hex(var, 0);
	}

	return 0;
}

int smarcid_env_prog(const struct digi_smarcid *smarcid)
{
	char cmd[80];
	int ret;

	ASSERT_SMARCID_WORD_LAYOUT();

	/* Set 'smarcid_n' variables from a given SMARCID */
	for (int i = 0; i < smarcid_nwords; i++) {
		sprintf(cmd, "setenv -f smarcid_%d %08x", i, ((u32 *) smarcid)[i]);
		ret = run_command(cmd, 0);
		if (ret)
			return -1;
	}

	/* Trigger a SMARCID-related variables update */
	board_smarcid_update();

	return 0;
}

int smarcid_env_clear(void)
{
	char cmd[80];
	int ret;

	/* Clear 'smarcid_n' variables */
	for (int i = 0; i < smarcid_nwords; i++) {
		sprintf(cmd, "setenv -f smarcid_%d", i);
		ret = run_command(cmd, 0);
		if (ret)
			return -1;
	}

	/* Trigger a SMARCID-related variables update */
	board_smarcid_update();

	return 0;
}

__weak char board_smarcid_get_location_char(int location_index)
{
	return '-';
}

void smarcid_get_serial_number(const struct digi_smarcid *smarcid)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	int ret;

	/* Skip function for SMT modules */
	if (!is_smarc(smarcid))
		return;

	sprintf(cmd, "setenv -f smarc_serial# %c%02d%02d%02d%06d",
		board_smarcid_get_location_char(smarcid->location),
		smarcid->year,
		smarcid->week,
		CONFIG_DIGI_FAMILY_ID,
		smarcid->sn);

	ret = run_command(cmd, 0);
	if (ret)
		printf("ERROR setting 'smarc_serial#' from SMARCID (%d)\n", ret);
}

void smarcid_get_variant(const struct digi_smarcid *smarcid)
{
	char var[200];

	/* Skip function for SMT modules */
	if (!is_smarc(smarcid))
		return;

	sprintf(var, "0x%02x", smarcid->variant);
	env_set("smarc_variant", var);
}

void fdt_fixup_fuse_smarcid(void *fdt)
{
	struct digi_smarcid smarcid;
	char str[20];
	int ret;

	ASSERT_SMARCID_WORD_LAYOUT();

	/* Register FUSE SMARCID words in the device tree */
	ret = board_smarcid_fuse_read(&smarcid);
	if (ret)
		return;

	/* Skip function for SMT modules */
	if (!is_smarc(&smarcid))
		return;

	for (int i = 0; i < smarcid_nwords; i++) {
		sprintf(str, "digi,smarcid_fuse_%d", i);
		do_fixup_by_path_u32(fdt, "/", str, *((u32 *)&smarcid + i), 1);
	}
}
