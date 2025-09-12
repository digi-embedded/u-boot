/*
 * Copyright (C) 2024-2025, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <command.h>
#include <common.h>
#include <fuse.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/hab.h>
#ifdef CONFIG_RNG_SELF_TEST
#include "../drivers/crypto/fsl/jr.h"
#endif

#include "../helper.h"
#include "encryption.h"
#include "hab.h"

#define RNG_FAIL_EVENT_SIZE	36
#define SW_RNG_TEST_FAILED	1
#define SW_RNG_TEST_PASSED	2
#define SW_RNG_TEST_NA		3

int rng_swtest_status = 0;

static uint8_t habv4_known_rng_fail_events[][RNG_FAIL_EVENT_SIZE] = {
	{ 0xdb, 0x00, 0x24, 0x42,  0x69, 0x30, 0xe1, 0x1d,
	  0x00, 0x80, 0x00, 0x02,  0x40, 0x00, 0x36, 0x06,
	  0x55, 0x55, 0x00, 0x03,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x01 },
	{ 0xdb, 0x00, 0x24, 0x42,  0x69, 0x30, 0xe1, 0x1d,
	  0x00, 0x04, 0x00, 0x02,  0x40, 0x00, 0x36, 0x06,
	  0x55, 0x55, 0x00, 0x03,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x01 },
};

extern enum hab_status hab_rvt_report_event(enum hab_status status, uint32_t index,
					    uint8_t *event, size_t *bytes);

static int hab_event_warning_check(uint8_t *event, size_t *bytes)
{
	int ret = SW_RNG_TEST_NA, i;
	bool is_rng_fail_event = false;

	/* Get HAB Event warning data */
	hab_rvt_report_event(HAB_WARNING, 0, event, bytes);

	/* Compare HAB event warning data with known Warning issues */
	for (i = 0; i < ARRAY_SIZE(habv4_known_rng_fail_events); i++) {
		if (memcmp(event, habv4_known_rng_fail_events[i],
			   RNG_FAIL_EVENT_SIZE) == 0) {
			is_rng_fail_event = true;
			break;
		}
	}

	if (is_rng_fail_event) {
#ifdef CONFIG_RNG_SELF_TEST
		printf("RNG:   self-test failure detected, will run software self-test\n");
		rng_self_test();
		ret = SW_RNG_TEST_PASSED;
#endif
	}

	return ret;
}

/*
 * Check HAB events at boot and if warnings are detected, verify if they
 * are related to the RNG self-test issue.
 */
void hab_verification(void)
{
	uint32_t ret;
	uint8_t event_data[36] = { 0 }; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config = 0;
	enum hab_state state = 0;
	hab_rvt_report_status_t *hab_report_status = (hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;

	/* HAB event verification */
	ret = hab_report_status(&config, &state);
	if (ret == HAB_WARNING) {
		pr_debug("\nHAB Configuration: 0x%02x, HAB State: 0x%02x\n",
		       config, state);
		/* Verify RNG self test */
		rng_swtest_status = hab_event_warning_check(event_data, &bytes);
		if (rng_swtest_status == SW_RNG_TEST_PASSED) {
			printf("RNG:   self-test failed, but software test passed.\n");
		} else if (rng_swtest_status == SW_RNG_TEST_FAILED) {
#ifdef CONFIG_RNG_SELF_TEST
			printf("WARNING: RNG self-test and software test failed!\n");
#else
			printf("WARNING: RNG self-test failed!\n");
#endif
			if (imx_hab_is_enabled()) {
				printf("Aborting secure boot.\n");
				run_command("reset", 0);
			}
		}
	} else {
		rng_swtest_status = SW_RNG_TEST_NA;
	}
}

__weak int get_dek_blob(ulong addr, u32 *size)
{
	return 1;
}

__weak int get_dek_blob_offset(ulong addr, ulong size, u32 *offset)
{
	return -1;
}

bool trustfence_is_closed(void)
{
	return imx_hab_is_enabled();
}

int trustfence_status(void)
{
	hab_rvt_report_status_t *hab_report_status = (hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;
	enum hab_config config = 0;
	enum hab_state state = 0;
	int ret;

	printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted() ?
			"[YES]" : "[NO]");
	puts("* HAB events:\t\t");
	ret = hab_report_status(&config, &state);
	if (ret == HAB_SUCCESS)
		puts("[NO ERRORS]\n");
	else if (ret == HAB_FAILURE)
		puts("[ERRORS PRESENT!]\n");
	else if (ret == HAB_WARNING) {
		if (rng_swtest_status == SW_RNG_TEST_PASSED) {
			puts("[NO ERRORS]\n");
			puts("\n");
			puts("Note: RNG selftest failed, but software test passed\n");
		} else {
			puts("[WARNINGS PRESENT!]\n");
		}
	}

	return 0;
}

static int disable_ext_mem_boot(void)
{
	return fuse_prog(CONFIG_TRUSTFENCE_DIRBTDIS_BANK,
			 CONFIG_TRUSTFENCE_DIRBTDIS_WORD,
			 1 << CONFIG_TRUSTFENCE_DIRBTDIS_OFFSET);
}

static int lock_srk_otp(void)
{
	return fuse_prog(CONFIG_TRUSTFENCE_SRK_OTP_LOCK_BANK,
			 CONFIG_TRUSTFENCE_SRK_OTP_LOCK_WORD,
			 1 << CONFIG_TRUSTFENCE_SRK_OTP_LOCK_OFFSET);
}

int close_device(int confirmed)
{
	hab_rvt_report_status_t *hab_report_status = (hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;
	enum hab_config config = 0;
	enum hab_state state = 0;
	int ret = -1;

	ret = hab_report_status(&config, &state);
	if (ret == HAB_FAILURE) {
		puts("[ERROR]\n There are HAB Events which will prevent the target from booting once closed.\n");
		puts("Run 'hab_status' and check the errors.\n");
		return CMD_RET_FAILURE;
	} else if (ret == HAB_WARNING) {
		if (rng_swtest_status == SW_RNG_TEST_FAILED) {
			puts("[WARNING]\n There are HAB warnings which could prevent the target from booting once closed.\n");
			puts("Run 'hab_status' and check the errors.\n");
			return CMD_RET_FAILURE;
		}
	}

	puts("Before closing the device DIR_BT_DIS will be burned.\n");
	puts("This permanently disables the ability to boot using external memory.\n");
	puts("The SRK_LOCK OTP bit will also be programmed, locking the SRK fields.\n");
	puts("Please confirm the programming of SRK_LOCK, DIR_BT_DIS and SEC_CONFIG[1]\n\n");
	if (!confirmed && !confirm_prog())
		return CMD_RET_FAILURE;

	puts("Programming DIR_BT_DIS eFuse...\n");
	if (disable_ext_mem_boot())
		goto err;
	puts("[OK]\n");

	puts("Programming SRK_LOCK eFuse...\n");
	if (lock_srk_otp())
		goto err;
	puts("[OK]\n");

	puts("Closing device...\n");
	return fuse_prog(CONFIG_TRUSTFENCE_CLOSE_BIT_BANK,
			 CONFIG_TRUSTFENCE_CLOSE_BIT_WORD,
			 1 << CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET);
err:
	return ret;
}

int sense_key_status(u32 *val)
{
	if (fuse_sense(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			val))
		return -1;

	*val = (*val >> CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET) &
		CONFIG_TRUSTFENCE_SRK_REVOKE_MASK;

	return 0;
}

int revoke_key_index(int i)
{
	u32 val = ((1 << i) & CONFIG_TRUSTFENCE_SRK_REVOKE_MASK) <<
		    CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET;
	return fuse_prog(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			 CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			 val);
}

/*
 * The boot artifacts signing script hardcodes the DEK blob address at a
 * fixed offset (0x100) before the kernel load address, so we need to restore
 * the DEK blob from the bootloader into that position to allow authenticating
 * the rest of the boot artifacts.
 */
void restore_dek_blob(void)
{
	/* Must match DEK_BLOB_OFFSET in the trustfence-sign-artifact script */
	const ulong DEK_BLOB_OFFSET = 0x100;
	ulong loadaddr = env_get_ulong("loadaddr", 16, CONFIG_SYS_LOAD_ADDR);

	get_dek_blob(loadaddr - DEK_BLOB_OFFSET, NULL);
}
