/*
 *  Copyright (C) 2020-2026, Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef TF_AUTH_H
#define TF_AUTH_H

#if defined(CONFIG_AUTH_FIT_ARTIFACT)
int digi_auth_image(ulong addr);
#else
int digi_auth_image(ulong *ddr_start, ulong raw_image_size);
#endif

#endif  /* TF_AUTH_H */
