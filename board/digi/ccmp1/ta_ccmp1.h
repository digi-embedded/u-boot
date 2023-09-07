/*
 * Copyright 2023 Digi International Inc
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#ifndef __TA_CCMP1_H__
#define __TA_CCMP1_H__

#if !defined(CONFIG_AES_KEY_LENGTH)
#define CONFIG_AES_KEY_LENGTH ""
#endif

/* The function IDs implemented in the associated TA */

/*
 * TA_AES_CMD_SET_KEY - Allocate resources for the AES ciphering
 * param[0] (value) Algorithmus
 * param[1] (value) Key size
 * param[2] (value) encryption mode (encrypt/decrypt)
 * param[3] unused
 */
#define TA_AES_CMD_PREPARE		0

/*
 * TA_AES_CMD_SET_KEY - Allocate resources for the AES ciphering
 * param[0] (memref) key data, size shall equal key length
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define TA_AES_CMD_SET_KEY		1

/*
 * TA_AES_CMD_SET_IV - reset IV
 * param[0] (memref) initial vector, size shall equal block length
 * param[1] unused
 * param[2] unused
 * param[3] unused
 */
#define TA_AES_CMD_SET_IV		2

/*
 * TA_AES_CMD_CIPHER - Cipher input buffer into output buffer
 * param[0] (memref) input buffer
 * param[1] (memref) output buffer (shall be bigger than input buffer)
 * param[2] unused
 * param[3] unused
 */
#define TA_AES_CMD_CIPHER	3

#define TA_AES_MODE_ENCODE	1
#define TA_AES_MODE_DECODE	0

#define TA_AES_ALGO_CTR		2

struct tee_ctx {
	struct udevice *tee;
	u32 tee_session;
};

struct tee_hw_unique_key {
	uint8_t data[16];
};

/* UUID of the TA */
#define TA_STM32MP_CRYP_UUID { 0xc2fad363, 0x5d9f, 0x4fc4, \
		{ 0xa4, 0x17, 0x55, 0x58, 0x41, 0xe0, 0x57, 0x45 } }

/* Functions */
int prepare_optee_session(struct tee_ctx *ctx);
int close_optee_session(struct tee_ctx *ctx);
int prepare_optee_aes(struct tee_ctx *ctx, int algo, int key_size, int mode);
int set_key(struct tee_ctx *ctx, size_t key_sz);
int set_iv(struct tee_ctx *ctx, char *iv, size_t iv_sz);
int cipher_buffer(struct tee_ctx *ctx, char *in, char *out, size_t cipher_sz);
int crypt_cipher_data(int enc, char *cipher_data, size_t size);

#endif /* __TA_CCMP1_H__ */
