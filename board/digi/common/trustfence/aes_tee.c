/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <malloc.h>
#include <tee.h>
#include <uboot_aes.h>

/* From TA's public header (aes_ta.h) */
#define TA_AES_UUID \
	{ 0xc2fad363, 0x5d9f, 0x4fc4, \
		{ 0xa4, 0x17, 0x55, 0x58, 0x41, 0xe0, 0x57, 0x45 } }
#define TA_AES_ALGO_ECB			0
#define TA_AES_ALGO_CBC			1
#define TA_AES_ALGO_CTR			2
#define TA_AES_SIZE_128BIT		(128 / 8)
#define TA_AES_SIZE_256BIT		(256 / 8)
#define TA_AES_MODE_DECODE		0
#define TA_AES_MODE_ENCODE		1
#define TA_AES_CMD_PREPARE		0
#define TA_AES_CMD_SET_KEY		1
#define TA_AES_CMD_SET_IV		2
#define TA_AES_CMD_CIPHER		3

struct tee_ctx {
	struct udevice *dev;
	u32 session;
};

static int open_session(struct tee_ctx *tee,
			const struct tee_optee_ta_uuid *uuid)
{
	struct tee_open_session_arg arg;

	/* Get tee device */
	tee->dev = tee_find_device(NULL, NULL, NULL, NULL);
	if (!tee->dev) {
		printf("Cannot get OP-TEE device\n");
		return -1;
	}

	memset(&arg, 0, sizeof(arg));
	tee_optee_ta_uuid_to_octets(arg.uuid, uuid);

	if (tee_open_session(tee->dev, &arg, 0, NULL) || arg.ret)
		return -1;

	tee->session = arg.session;

	return 0;
}

static int prepare_aes(struct tee_ctx *tee, int key_size, int enc)
{
	struct tee_invoke_arg arg;
	struct tee_param param[3];
	int params_size = sizeof(param) / sizeof(struct tee_param);

	memset(&arg, 0, sizeof(arg));
	arg.func = TA_AES_CMD_PREPARE;
	arg.session = tee->session;

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = TA_AES_ALGO_CTR;
	param[1].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = key_size;
	param[2].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[2].u.value.a = enc ? TA_AES_MODE_ENCODE : TA_AES_MODE_DECODE;;

	if (tee_invoke_func(tee->dev, &arg, params_size, param) || arg.ret)
		return -1;

	return 0;
}

static int set_key(struct tee_ctx *tee, int key_size)
{
	struct tee_invoke_arg arg;
	struct tee_param param[1];
	int params_size = sizeof(param) / sizeof(struct tee_param);

	memset(&arg, 0, sizeof(arg));
	arg.func = TA_AES_CMD_SET_KEY;
	arg.session = tee->session;

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = key_size;

	if (tee_invoke_func(tee->dev, &arg, params_size, param) || arg.ret)
		return -1;

	return 0;
}

static int set_iv(struct tee_ctx *tee, uint8_t *iv, int iv_size)
{
	struct tee_invoke_arg arg;
	struct tee_param param[1];
	int params_size = sizeof(param) / sizeof(struct tee_param);
	struct tee_shm *iv_shm;
	int ret = 0;

	memset(&arg, 0, sizeof(arg));
	arg.func = TA_AES_CMD_SET_IV;
	arg.session = tee->session;

	if (tee_shm_register(tee->dev, iv, iv_size, 0, &iv_shm))
		return -1;

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = iv_shm;
	param[0].u.memref.size = iv_shm->size;

	if (tee_invoke_func(tee->dev, &arg, params_size, param) || arg.ret)
		ret = -1;

	tee_shm_free(iv_shm);

	return ret;
}

static int cipher_buffer(struct tee_ctx *tee, uint8_t *in, uint8_t *out,
			 int size)
{
	struct tee_invoke_arg arg;
	struct tee_param param[2];
	int params_size = sizeof(param) / sizeof(struct tee_param);
	struct tee_shm *in_shm, *out_shm;
	int ret = 0;

	memset(&arg, 0, sizeof(arg));
	arg.func = TA_AES_CMD_CIPHER;
	arg.session = tee->session;

	if (tee_shm_register(tee->dev, in, size, 0, &in_shm)
	    || tee_shm_register(tee->dev, out, size, 0, &out_shm))
		return -1;

	memset(param, 0, sizeof(param));
	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = in_shm;
	param[0].u.memref.size = in_shm->size;
	param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[1].u.memref.shm = out_shm;
	param[1].u.memref.size = out_shm->size;

	if (tee_invoke_func(tee->dev, &arg, params_size, param) || arg.ret)
		ret = -1;

	tee_shm_free(in_shm);
	tee_shm_free(out_shm);

	return ret;
}

int optee_crypt_data(int enc, uint8_t *key_mod, uint8_t *data, u32 len)
{
	struct tee_ctx tee;
	const struct tee_optee_ta_uuid aes_uuid = TA_AES_UUID;
	const int key_size = AES256_KEY_LENGTH;
	uint8_t *cryptdata;

	/* Open session */
	if (open_session(&tee, &aes_uuid)) {
		printf("Error: %s failed to open TA session\n", __func__);
		return -1;
	}

	/* Prepare AES */
	prepare_aes(&tee, key_size, enc);

	/* Set key */
	set_key(&tee, key_size);

	/* Set IV */
	set_iv(&tee, key_mod, AES_BLOCK_LENGTH);

	/* Encrypt/Decrypt buffer */
	cryptdata = calloc(1, len);
	if (cryptdata) {
		cipher_buffer(&tee, data, cryptdata, len);
		memcpy(data, cryptdata, len);
		free(cryptdata);
	}

	tee_close_session(tee.dev, tee.session);

	return 0;
}
