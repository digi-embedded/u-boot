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

#ifdef CONFIG_RNG_SELF_TEST
extern int rng_swtest_status;
#endif

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
#ifdef CONFIG_RNG_SELF_TEST
		if (rng_swtest_status == SW_RNG_TEST_PASSED) {
			puts("[NO ERRORS]\n");
			puts("\n");
			puts("Note: RNG selftest failed, but software test passed\n");
		} else {
#endif
			puts("[WARNINGS PRESENT!]\n");
#ifdef CONFIG_RNG_SELF_TEST
		}
#endif
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
#ifdef CONFIG_RNG_SELF_TEST
		if (rng_swtest_status == SW_RNG_TEST_FAILED) {
#endif
			puts("[WARNING]\n There are HAB warnings which could prevent the target from booting once closed.\n");
			puts("Run 'hab_status' and check the errors.\n");
			return CMD_RET_FAILURE;
#ifdef CONFIG_RNG_SELF_TEST
		}
#endif
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
