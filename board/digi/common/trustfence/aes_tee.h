/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef TF_AES_TEE_H
#define TF_AES_TEE_H

int optee_crypt_data(int enc, uint8_t *key_mod, uint8_t *data, u32 len);

#endif /* TF_AES_TEE_H */
