/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2026 NXP
 */

#ifndef __IMX_QB_H__
#define __IMX_QB_H__

#include <stdbool.h>

bool qb_check(void);
int  qb(int qb_dev, int qb_bootdev, bool save);
void spl_qb_save(void);

#endif
