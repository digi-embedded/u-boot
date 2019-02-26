/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8X_H
#define CCIMX8X_H

/* Common ccimx8x functions */
int ccimx8_init(void);
int ccimx8x_late_init(void);
void print_ccimx8x_info(void);
void som_default_environment(void);
void fdt_fixup_ccimx8x(void *fdt);

#endif  /* CCIMX8X_H */
