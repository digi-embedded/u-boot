/*
 * Copyright (C) 2024, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <env.h>
#include <fdt_support.h>
#include <fuse.h>
#include <mapmem.h>

#include "../helper.h"
#include "boot.h"

/*
 * Check if all SRK words have been burned.
 *
 * Returns:
 * 0 if all SRK are burned
 * i+1 if SRK word i is not burned
 * <0 on error
 */
__weak int fuse_check_srk(void)
{
	int i;
	u32 val;
	int bank, word;

	for (i = 0; i < CONFIG_TRUSTFENCE_SRK_WORDS; i++) {
		bank = CONFIG_TRUSTFENCE_SRK_BANK +
		       (i / CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK);
		word = i % CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK;
		if (fuse_sense(bank, word+CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET,
			       &val))
			return -1;
		if (val == 0)
			return i + 1;
	}

	return 0;
}

__weak int fuse_prog_srk(u32 addr, u32 size)
{
	int i;
	int ret;
	int bank, word;
	uint32_t *src_addr = map_sysmem(addr, size);

	if (size != CONFIG_TRUSTFENCE_SRK_WORDS * 4) {
		puts("Bad size\n");
		return -1;
	}

#ifdef CONFIG_CC8X
	env_set("force_prog_ecc", "yes");
#endif

	for (i = 0; i < CONFIG_TRUSTFENCE_SRK_WORDS; i++) {
		bank = CONFIG_TRUSTFENCE_SRK_BANK +
		       (i / CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK);
		word = i % CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK;
		ret = fuse_prog(bank, word+CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET,
				src_addr[i]);
		if (ret)
			break;
	}

#ifdef CONFIG_CC8X
	env_set("force_prog_ecc", NULL);
#endif

	return ret;
}

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
