/*
 * Copyright 2023 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/mach-imx/ele_api.h>

#include "../common/trustfence.h"

/* see arch/arm/mach-imx/ele_ahab.c */
#define AHAB_MAX_EVENTS 8
int confirm_close(void);

/*
 * Trustfence helper functions
 */

int close_device(int confirmed_close)
{
	int err;
	u32 lc;
	u32 events[AHAB_MAX_EVENTS];
	u32 cnt = AHAB_MAX_EVENTS;

	if (!confirmed_close && !confirm_close())
		return -EACCES;

	lc = readl(FSB_BASE_ADDR + 0x41c);
	lc &= 0x3ff;

	if (lc != 0x8) {
		printf
		    ("Current lifecycle is NOT OEM open, can't move to OEM closed (0x%x)\n",
		     lc);
		return -EPERM;
	}

	err = ahab_get_events(events, &cnt, NULL);
	if (err || cnt) {
		printf("AHAB events found!\n");
		return -EIO;
	}

	err = ahab_forward_lifecycle(8, NULL);
	if (err != 0) {
		printf("Error in forward lifecycle to OEM closed\n");
		return -EIO;
	}
	printf("Device closed successfully\n");

	return 0;
}

bool imx_hab_is_enabled(void)
{
	uint32_t lc;

	lc = readl(FSB_BASE_ADDR + 0x41c);
	lc &= 0x3f;

	if (lc != 0x20)
		return false;
	else
		return true;
}

int trustfence_status(void)
{
	u32 events[AHAB_MAX_EVENTS];
	u32 cnt = AHAB_MAX_EVENTS;
	int ret;

	printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted()?
	       "[YES]" : "[NO]");
	puts("* AHAB events:\t\t");
	ret = ahab_get_events(events, &cnt, NULL);
	if (ret)
		puts("[GET EVENTS FAILURE]\n");
	else if (!cnt)
		puts("[NO ERRORS]\n");
	else
		puts("[ERRORS PRESENT!]\n");

	return 0;
}

/* TODO: not yet implemented */
int sense_key_status(u32 * val)
{
	*val = 0;

	return 0;
}
