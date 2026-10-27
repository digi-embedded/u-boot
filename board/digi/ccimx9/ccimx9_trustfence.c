/*
 * Copyright 2023-2026 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <asm/arch/imx-regs.h>
#include <asm/io.h>
#include <asm/mach-imx/ele_api.h>
#include <linux/errno.h>

#include "../common/trustfence.h"

/* see arch/arm/mach-imx/ele_ahab.c */
#define AHAB_MAX_EVENTS 8
#if IS_ENABLED(CONFIG_IMX95)
#define FSB_LC_OFFSET 0x414
#define LC_OEM_OPEN 0x10
#define LC_OEM_CLOSED 0x40
#else
#define FSB_LC_OFFSET 0x41c
#define LC_OEM_OPEN 0x8
#define LC_OEM_CLOSED 0x20
#endif
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

	lc = readl(FSB_BASE_ADDR + FSB_LC_OFFSET);
	lc &= 0x3ff;

	if (lc != LC_OEM_OPEN) {
		printf
		    ("Current lifecycle is NOT OEM open, can't move to OEM closed (0x%x)\n",
		     lc);
		return -EPERM;
	}

	err = ele_get_events(events, &cnt, NULL);
	if (err || cnt) {
		printf("AHAB events found!\n");
		return -EIO;
	}

	err = ele_forward_lifecycle(8, NULL);
	if (err != 0) {
		printf("Error in forward lifecycle to OEM closed\n");
		return -EIO;
	}
	printf("Device closed successfully\n");

	return 0;
}

bool trustfence_is_closed(void)
{
	u32 lc = readl(FSB_BASE_ADDR + FSB_LC_OFFSET) & 0x3ff;

	return lc == LC_OEM_CLOSED;
}

int trustfence_status(void)
{
	u32 events[AHAB_MAX_EVENTS];
	u32 cnt = AHAB_MAX_EVENTS;
	int ret;

	if (!is_usb_boot())
		printf("* %-*s%s\n", TF_STATUS_ALIGN, "Encrypted U-Boot:",
			is_uboot_encrypted() ? "[YES]" : "[NO]");
	printf("* %-*s", TF_STATUS_ALIGN, "AHAB events:");
	ret = ele_get_events(events, &cnt, NULL);
	if (ret)
		puts("[GET EVENTS FAILURE]\n");
	else if (!cnt)
		puts("[NO ERRORS]\n");
	else
		puts("[ERRORS PRESENT!]\n");

	return 0;
}

int revoke_keys(void)
{
	u32 index = 0x10 /* SRK revocation of the OEM container */ ;
	u32 info_type;
	int ret = -1;

	if (ele_commit(index, NULL, &info_type))
		goto err;

	ret = (index != info_type);
	if (ret)
		goto err;

err:
	return ret ? -1 : 0;
}

/*
 * TODO: so far NXP has not provided the TRUSTFENCE_SRK_REVOKE fuse
 * information, so read the mask again from the container.
 */
int sense_key_status(u32 *val)
{
	int ret = -1;

	/* Container Header can only be read from the storage media */
	if (is_usb_boot())
		printf
		    ("Reading revoke mask is not supported booting from USB.\n");
	else
		ret = get_srk_revoke_mask(val);

	return ret ? -1 : 0;
}
