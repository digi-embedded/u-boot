/*
 * Copyright (C) 2019 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8M_H
#define CCIMX8M_H

/* Common ccimx8m functions */
int ccimx8_init(void);
void print_ccimx8m_info(void);
void som_default_environment(void);
void fdt_fixup_ccimx8m(void *fdt);

#endif  /* CCIMX8M_H */
