/*
 * Copyright (C) 2025, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef TF_HAB_H
#define TF_HAB_H

void hab_verification(void);
int get_dek_blob_offset(ulong addr, ulong size, u32 *offset);
int get_dek_blob(ulong addr, u32 *size);
int revoke_key_index(int i);
void restore_dek_blob(void);

#endif /* TF_HAB_H */
