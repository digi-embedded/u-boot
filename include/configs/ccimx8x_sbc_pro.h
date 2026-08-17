/*
 * Copyright 2017 NXP
 * Copyright (C) 2018,2019 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8X_SBC_PRO_CONFIG_H
#define CCIMX8X_SBC_PRO_CONFIG_H

#include <linux/stringify.h>
#include "ccimx8x_common.h"

#define CONFIG_BOARD_DESCRIPTION	"SBC Pro"

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

/* Android specific configuration */
#if defined(CONFIG_ANDROID_SUPPORT)
#include "ccimx8x_sbc_pro_android.h"
#endif

#endif /* CCIMX8X_SBC_PRO_CONFIG_H */
