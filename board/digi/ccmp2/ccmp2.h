/*
 * Copyright (C) 2024 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCMP2_H
#define CCMP2_H

/* Common ccmp2 functions */
int ccmp2_init(void);
void fdt_fixup_ccmp2(void *fdt);
void som_default_environment(void);
void print_som_info(void);
void print_bootinfo(void);
bool is_usb_boot(void);

#endif  /* CCMP1_H */
