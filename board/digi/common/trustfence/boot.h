/*
 * Copyright (C) 2025-2026, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef TF_BOOT_H
#define TF_BOOT_H

int fuse_check_srk(void);
int fuse_prog_srk(u32 addr, u32 size);
#if defined(CONFIG_IMX95) && defined(CONFIG_IMX_PQC_SUPPORT)
int fuse_check_pqc_srk(void);
int fuse_prog_pqc_srk(u32 addr, u32 size);
#endif
void fdt_fixup_trustfence(void *fdt);
int sense_key_status(u32 *val);
int close_device(int confirmed);
int trustfence_status(void);
bool trustfence_is_closed(void);

#endif /* TF_BOOT_H */
