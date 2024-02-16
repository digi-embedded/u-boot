/*
 * Copyright 2022 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX93_H
#define CCIMX93_H

int ccimx93_init(void);
void som_default_environment(void);
int ccimx93_late_init(void);
void fdt_fixup_ccimx93(void *fdt);
void print_som_info(void);
void print_bootinfo(void);

#endif  /* CCIMX93_H */
