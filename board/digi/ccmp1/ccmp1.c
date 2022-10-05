// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2022, Digi International Inc - All Rights Reserved
 */
#include <common.h>
#include <env.h>
#include <env_internal.h>

#include "../common/helper.h"
#include "ccmp1.h"
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio)
		return ENVL_UNKNOWN;

	if (CONFIG_IS_ENABLED(ENV_IS_IN_UBI))
		return ENVL_UBI;
	else
		return ENVL_NOWHERE;
}

static bool board_has_wireless(void)
{
	return true; /* assume it has, by default */
}

static bool board_has_bluetooth(void)
{
	return true; /* assume it has, by default */
}

void fdt_fixup_ccmp1(void *fdt)
{
	if (board_has_wireless()) {
		/* Wireless MACs */
		fdt_fixup_mac(fdt, "wlanaddr", "/wireless", "mac-address");
		fdt_fixup_mac(fdt, "wlan1addr", "/wireless", "mac-address1");
		fdt_fixup_mac(fdt, "wlan2addr", "/wireless", "mac-address2");
		fdt_fixup_mac(fdt, "wlan3addr", "/wireless", "mac-address3");

		/* Regulatory domain */
		fdt_fixup_regulatory(fdt);
	}

	if (board_has_bluetooth())
		fdt_fixup_mac(fdt, "btaddr", "/bluetooth", "mac-address");

	fdt_fixup_uboot_info(fdt);
}
