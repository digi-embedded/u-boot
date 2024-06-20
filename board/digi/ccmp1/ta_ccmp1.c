// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2023, Digi International Inc - All Rights Reserved
 */

#include <env_internal.h>
#include <errno.h>
#include <linux/stddef.h>
#include <stdlib.h>
#include <string.h>
#include <tee.h>
#include <uboot_aes.h>
#include <u-boot/md5.h>
#include "ta_ccmp1.h"
#include "../common/hwid.h"

int prepare_optee_session(struct tee_ctx *ctx) {
	struct udevice *tee = NULL;
	const struct tee_optee_ta_uuid uuid = TA_STM32MP_CRYP_UUID;
	struct tee_open_session_arg arg = { };
	int res = TEE_SUCCESS;

	tee = tee_find_device(tee, NULL, NULL, NULL);
	if (!tee)
		return -ENODEV;

	memset(&arg, 0, sizeof(arg));
	tee_optee_ta_uuid_to_octets(arg.uuid, &uuid);
	/* Use kernel login */
	arg.clnt_login = TEE_LOGIN_REE_KERNEL;
	/* Open TA seesion */
	res = tee_open_session(tee, &arg, 0, NULL);
	if (res < 0 || arg.ret != 0) {
		if (!res) {
			res = -EIO;
			ctx->tee_session = 0;
			printf("Error: failed to open Tee session\n");
		}
	} else {
		ctx->tee = tee;
		ctx->tee_session = arg.session;
	}

	return res;
}

int close_optee_session(struct tee_ctx *ctx) {
	int res = TEE_SUCCESS;

	if (!ctx->tee)
		return -ENODEV;

	res = tee_close_session(ctx->tee, ctx->tee_session);
	if (res)
		return res;

	ctx->tee = NULL;
	ctx->tee_session = 0;

	return res;
}

int prepare_optee_aes(struct tee_ctx *ctx, int algo, int key_size, int mode) {
	struct tee_invoke_arg arg;
	struct tee_param param[3];
	int res = TEE_SUCCESS;

	memset(&arg, 0, sizeof(arg));
	memset(param, 0, sizeof(param));
	arg.func = TA_AES_CMD_PREPARE;
	arg.session = ctx->tee_session;

	/* Prepare encryption/decryption configuration */
	param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = algo;
	param[1].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[1].u.value.a = key_size;
	param[2].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[2].u.value.a = mode;
	res =  tee_invoke_func(ctx->tee, &arg, sizeof(param)/sizeof(struct tee_param), param);
	if (res < 0 || arg.ret != 0) {
		printf("%s TA_AES_CMD_PREPARE invoke failed TEE err: %x, err:%x \n", __func__, arg.ret, res);
		if (!res)
			res = -EIO;
	}

	return res;
}

int set_key(struct tee_ctx *ctx, size_t key_sz) {
	struct tee_invoke_arg arg;
	struct tee_param param[1];
	int res = TEE_SUCCESS;

	memset(param, 0, sizeof(param));
	memset(&arg, 0, sizeof(arg));
	arg.func = TA_AES_CMD_SET_KEY;
	arg.session = ctx->tee_session;

	param[0].attr = TEE_PARAM_ATTR_TYPE_VALUE_INPUT;
	param[0].u.value.a = key_sz;

	res = tee_invoke_func(ctx->tee, &arg, sizeof(param)/sizeof(struct tee_param), param);
	if (res < 0 || arg.ret != 0) {
		printf("%s TA_AES_CMD_SET_KEY invoke failed TEE err: %x, err:%x \n", __func__,arg.ret, res);
		if (!res)
			res = -EIO;
	}

	return res;
}

int set_iv(struct tee_ctx *ctx, char *iv, size_t iv_sz) {
	struct tee_invoke_arg arg;
	struct tee_param param[1];
	struct tee_shm *iv_shm;
	int res = TEE_SUCCESS;

	memset(&arg, 0, sizeof(arg));
	memset(param, 0, sizeof(param));
	arg.func = TA_AES_CMD_SET_IV;
	arg.session = ctx->tee_session;

	res = tee_shm_register(ctx->tee, iv, iv_sz, 0, &iv_shm);
	if (res)
		return res;
	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = iv_shm;
	param[0].u.memref.size = iv_sz;
	res = tee_invoke_func(ctx->tee, &arg, sizeof(param)/sizeof(struct tee_param), param);
	if (res < 0 || arg.ret != 0) {
		printf("%s TA_AES_CMD_SET_IV invoke failed TEE err: %x, err:%x \n", __func__,arg.ret, res);
		if (!res)
			res = -EIO;
	}

	/* free shared memory */
	tee_shm_free(iv_shm);

	return res;
}

int cipher_buffer(struct tee_ctx *ctx, char *in, char *out, size_t cipher_sz) {
	struct tee_invoke_arg arg;
	struct tee_param param[2];
	struct tee_shm *in_shm, *out_shm;
	int res = TEE_SUCCESS;

	memset(param, 0, sizeof(param));
	memset(&arg, 0, sizeof(arg));
	arg.func = TA_AES_CMD_CIPHER;
	arg.session = ctx->tee_session;

	res = tee_shm_register(ctx->tee, in, cipher_sz, 0, &in_shm);
	if (res) {
		printf("%s Could not allocate shared memory %x\n", __func__, res);
		goto err;
	}

	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = in_shm;
	param[0].u.memref.size = cipher_sz;

	res = tee_shm_register(ctx->tee, out, cipher_sz, 0, &out_shm);
	if (res) {
		printf("%s Could not allocate shared memory %x \n", __func__, res);
		goto err;
	}

	param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[1].u.memref.shm = out_shm;
	param[1].u.memref.size = cipher_sz;

	res = tee_invoke_func(ctx->tee, &arg, sizeof(param)/sizeof(struct tee_param), param);
	if (res < 0 || arg.ret != 0) {
		printf("%s TA_AES_CMD_CIPHER invoke failed TEE err: %x, err:%x \n", __func__,arg.ret, res);
		if (!res)
			res = -EIO;
	}

err:
	/* free shared memory */
	tee_shm_free(in_shm);
	tee_shm_free(out_shm);

	return res;
}

int crypt_cipher_data(int enc, char *cipher_data, size_t size)
{
	u8 *data, *iv;
	struct tee_ctx *ctx;
	int ret = 0;
	static struct digi_hwid my_hwid;

	/* Allocate all the buffer */
	iv = calloc(AES_BLOCK_LENGTH, sizeof(u8));
	data = calloc(size, sizeof(char));
	ctx = (struct tee_ctx*)malloc(sizeof(struct tee_ctx));

	if (!iv || !data || !ctx) {
		printf("%s: can't allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	/* Read HWID */
	board_read_hwid(&my_hwid);
	/* apply MD5SUM to the HWID */
	md5((unsigned char *)(&my_hwid), sizeof(my_hwid), iv);

	/* Open Optee session */
	ret = prepare_optee_session(ctx);
	if (ret) {
		printf("Error: %s Failed to open TA session\n", __func__);
		goto err;
	}

	/* Prepare decryption process */
	prepare_optee_aes(ctx, TA_AES_ALGO_CTR, AES256_KEY_LENGTH, enc);
	/* set key */
	set_key(ctx, AES256_KEY_LENGTH);
	/* set IV */
	set_iv(ctx, iv, AES_BLOCK_LENGTH);
	/* Encrypt buffer */
	cipher_buffer(ctx, cipher_data, data, size);
	/* copy ciphered data back */
	memcpy(cipher_data, data, ENV_SIZE);

err:
	/* Close Optee session */
	close_optee_session(ctx);
	/* free allocated buffer */
	free(iv);
	free(data);
	free(ctx);

	return ret;
}
