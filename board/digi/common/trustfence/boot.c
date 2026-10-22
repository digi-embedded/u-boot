/*
 * Copyright (C) 2024-2026, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>
#include <env.h>
#include <fdt_support.h>
#include <fuse.h>
#include <mapmem.h>

#include "boot.h"

/*
 * Check if all SRK words have been burned.
 *
 * Returns:
 * 0 if all SRK are burned
 * i+1 if SRK word i is not burned
 * <0 on error
 */
static int check_srk_fuses(int first_bank, int words, int words_per_bank,
			   int words_offset)
{
	int i;
	u32 val;
	int bank, word;

	for (i = 0; i < words; i++) {
		bank = first_bank + (i / words_per_bank);
		word = i % words_per_bank;
		if (fuse_sense(bank, word + words_offset, &val))
			return -1;
		if (val == 0)
			return i + 1;
	}

	return 0;
}

int fuse_check_srk(void)
{
	return check_srk_fuses(CONFIG_TRUSTFENCE_SRK_BANK,
			       CONFIG_TRUSTFENCE_SRK_WORDS,
			       CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK,
			       CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET);
}

static int prog_srk_fuses(int first_bank, int words, int words_per_bank,
			  int words_offset, u32 addr, u32 size)
{
	int i;
	int ret;
	int bank, word;
	u32 *src_addr;

	if (size != words * 4) {
		puts("Bad size\n");
		return -1;
	}

	src_addr = map_sysmem(addr, size);

	if (IS_ENABLED(CONFIG_CC8X))
		env_set("force_prog_ecc", "yes");

	for (i = 0; i < words; i++) {
		bank = first_bank + i / words_per_bank;
		word = i % words_per_bank;
		ret = fuse_prog(bank, word + words_offset, src_addr[i]);
		if (ret)
			break;
	}

	if (IS_ENABLED(CONFIG_CC8X))
		env_set("force_prog_ecc", NULL);

	return ret;
}

int fuse_prog_srk(u32 addr, u32 size)
{
	return prog_srk_fuses(CONFIG_TRUSTFENCE_SRK_BANK,
			      CONFIG_TRUSTFENCE_SRK_WORDS,
			      CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK,
			      CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET, addr, size);
}

#if defined(CONFIG_IMX95) && defined(CONFIG_IMX_PQC_SUPPORT)
int fuse_check_pqc_srk(void)
{
	return check_srk_fuses(CONFIG_TRUSTFENCE_PQC_SRK_BANK,
			       CONFIG_TRUSTFENCE_PQC_SRK_WORDS,
			       CONFIG_TRUSTFENCE_PQC_SRK_WORDS_PER_BANK,
			       CONFIG_TRUSTFENCE_PQC_SRK_WORDS_OFFSET);
}

int fuse_prog_pqc_srk(u32 addr, u32 size)
{
	return prog_srk_fuses(CONFIG_TRUSTFENCE_PQC_SRK_BANK,
			      CONFIG_TRUSTFENCE_PQC_SRK_WORDS,
			      CONFIG_TRUSTFENCE_PQC_SRK_WORDS_PER_BANK,
			      CONFIG_TRUSTFENCE_PQC_SRK_WORDS_OFFSET, addr, size);
}
#endif

void fdt_fixup_trustfence(void *fdt)
{
	/* Environment encryption is not enabled on open devices */
	if (!trustfence_is_closed()) {
		do_fixup_by_path(fdt, "/", "digi,tf-open", NULL, 0, 1);
		return;
	}

	if (IS_ENABLED(CONFIG_ENV_ENCRYPT))
		do_fixup_by_path(fdt, "/", "digi,uboot-env,encrypted", NULL, 0,
				 1);
	if (IS_ENABLED(CONFIG_OPTEE_ENV_ENCRYPT))
		do_fixup_by_path(fdt, "/", "digi,uboot-env,encrypted-optee",
				 NULL, 0, 1);

	do_fixup_by_path(fdt, "/", "digi,tf-closed", NULL, 0, 1);
}

__weak int sense_key_status(u32 * val) { return -1; }

__weak int close_device(int confirmed) { return -1; }

__weak int trustfence_status(void)  { return -1; }

__weak bool trustfence_is_closed(void) { return false; }
