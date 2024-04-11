/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef TF_AHAB_H
#define TF_AHAB_H

#define AHAB_BLOB_HDR_TAG	0x81
#define AHAB_BLOB_HDR_VER	0x00
#define AHAB_CNTR_HDR_TAG	0x87
#define AHAB_CNTR_HDR_VER	0x00
#define AHAB_SIGN_HDR_TAG	0x90
#define AHAB_SIGN_HDR_VER	0x00

int get_dek_blob_offset(ulong addr, u32 *offset);
int get_dek_blob_size(ulong addr, u32 *size);
int get_dek_blob(ulong addr, u32 *size);
bool is_container_encrypted(ulong addr, ulong *dek_addr);

#endif /* TF_AHAB_H */
