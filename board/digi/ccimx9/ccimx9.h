/*
 * Copyright 2022 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX9_H
#define CCIMX9_H

int ccimx9_init(void);
void som_default_environment(void);
int ccimx9_late_init(void);
void fdt_fixup_ccimx9(void *fdt);
void print_som_info(void);
void print_bootinfo(void);

#endif  /* CCIMX9_H */
