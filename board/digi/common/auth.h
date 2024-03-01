/*
 *  Copyright (C) 2020 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __AUTH_H
#define __AUTH_H

#if defined(CONFIG_AUTH_DISCRETE_ARTIFACTS)
int digi_auth_image(ulong *ddr_start, ulong raw_image_size);
#elif defined(CONFIG_AUTH_FIT_ARTIFACT)
int digi_auth_image(ulong addr);
#endif
int get_os_container_img_offset(ulong addr);

#endif  /* __AUTH_H */
