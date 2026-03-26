// SPDX-License-Identifier:	GPL-2.0-or-later
/*
 * Copyright (C) 2026, Digi International Inc.
 */

#include <mmc.h>

uint mmc_get_env_part(struct mmc *mmc)
{
	if (IS_SD(mmc))
		return 0;
	else
		return CONFIG_SYS_MMC_ENV_PART;
}
