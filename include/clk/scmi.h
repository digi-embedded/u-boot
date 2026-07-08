/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2025 NXP
 */

#ifndef _CLK_SCMI_H
#define _CLK_SCMI_H

#include <linux/types.h>

int scmi_clk_resolve_attr(ulong id, u32 *ctrl_flags);
#endif
