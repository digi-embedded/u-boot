/*
 * Copyright (C) 2022 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCMP1_H
#define CCMP1_H

/* Common ccmp1 functions */
int ccmp1_init(void);
void fdt_fixup_ccmp1(void *fdt);
void som_default_environment(void);
void print_som_info(void);

#endif  /* CCMP1_H */
