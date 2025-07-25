/*
 *  Copyright (C) 2016-2025, Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef TRUSTFENCE_H
#define TRUSTFENCE_H

#ifdef CONFIG_HAS_TRUSTFENCE
#include "trustfence/boot.h"
#include "trustfence/encryption.h"

#if defined(CONFIG_AUTH_ARTIFACTS) || defined(CONFIG_AUTHENTICATE_SQUASHFS_ROOTFS)
#include "trustfence/auth.h"
#endif
#ifdef CONFIG_AHAB_BOOT
#include "trustfence/ahab.h"
#endif
#ifdef CONFIG_IMX_HAB
#include "trustfence/hab.h"
#endif
#ifdef CONFIG_ENV_ENCRYPT
#include "trustfence/env.h"
#endif
#ifdef CONFIG_CONSOLE_DISABLE
#include "trustfence/console.h"
#endif
#ifdef CONFIG_TRUSTFENCE_JTAG
#include "trustfence/jtag.h"
#endif
#endif /* CONFIG_HAS_TRUSTFENCE */
#endif /* TRUSTFENCE_H */
