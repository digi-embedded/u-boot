// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2022, Digi International Inc - All Rights Reserved
 */
#include <common.h>
#include <env.h>
#include <env_internal.h>

enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio)
		return ENVL_UNKNOWN;

	if (CONFIG_IS_ENABLED(ENV_IS_IN_UBI))
		return ENVL_UBI;
	else
		return ENVL_NOWHERE;
}
