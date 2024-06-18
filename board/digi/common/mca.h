/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef MCA_H
#define MCA_H

#include "hwid.h"

/* MCA access functions */
int mca_read_reg(int reg, unsigned char *value);
int mca_bulk_read(int reg, unsigned char *values, int len);
int mca_write_reg(int reg, unsigned char value);
int mca_bulk_write(int reg, unsigned char *values, int len);
int mca_update_bits(int reg, unsigned char mask, unsigned char val);

void mca_reset(void);
void mca_save_cfg(void);
void mca_somver_update(const struct digi_hwid *hwid);
void mca_init(void);

#endif /* MCA_H */
