/*
 * Copyright (C) 2019-2020 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8_H
#define CCIMX8_H

/* Common ccimx8 functions */
int mmc_get_bootdevindex(void);
int ccimx8_init(void);
void print_som_info(void);
void som_default_environment(void);
int ccimx8_late_init(void);
void fdt_fixup_ccimx8(void *fdt);

#ifdef CONFIG_CC8X
int hwid_in_db(int variant);
void fdt_fixup_ccimx8x(void *fdt);
#endif

#endif  /* CCIMX8_H */
