/*
 * Copyright (C) 2016 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX6UL_H
#define CCIMX6UL_H

/* Common ccimx6ul functions */
int power_init_ccimx6ul(void);
int ccimx6ul_init(void);
int ccimx6ul_late_init(void);
void print_ccimx6ul_info(void);
int mca_read_reg(int reg, unsigned char *value);
int mca_bulk_read(int reg, unsigned char *values, int len);
int mca_write_reg(int reg, unsigned char value);
int mca_bulk_write(int reg, unsigned char *values, int len);
int mca_update_bits(int reg, unsigned char mask, unsigned char val);
void fdt_fixup_ccimx6ul(void *fdt);
void som_default_environment(void);

#endif  /* CCIMX6UL_H */
