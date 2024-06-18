/*
 *  Copyright (C) 2023 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#include <env_internal.h>
#include <u-boot/md5.h>

#ifdef CONFIG_ENV_AES_CAAM_KEY
#include <fuse.h>
#include <fsl_caam.h>
#include <asm/mach-imx/hab.h>
#include "../board/digi/common/trustfence.h"
#elif CONFIG_ENV_AES_CCMP1
#if !defined(CONFIG_AES_KEY_LENGTH)
#define CONFIG_AES_KEY_LENGTH ""
#endif
/* AES encryption support */
#include <nand.h>
#include <uboot_aes.h>
#include <tee.h>
#include "../board/digi/common/hwid.h"
#include "../board/digi/ccmp1/ta_ccmp1.h"
#include "../cmd/legacy-mtd-utils.h"
#endif

#ifdef CONFIG_ENV_AES_CAAM_KEY
int env_aes_cbc_crypt(env_t *env, const int enc)
{
	unsigned char *data = env->data;
	unsigned char *buffer;
	int ret = 0;
	unsigned char key_modifier[KEY_MODIFER_SIZE] = {0};

	if (!imx_hab_is_enabled())
		return 0;

	ret = get_trustfence_key_modifier(key_modifier);
	if (ret)
		return ret;

	caam_open();
	buffer = malloc(ENV_SIZE);
	if (!buffer) {
		debug("Not enough memory for en/de-cryption buffer");
		return -ENOMEM;
	}

	if (enc)
		ret = caam_gen_blob((ulong)data, (ulong)buffer, key_modifier, ENV_SIZE - BLOB_OVERHEAD);
	else
		ret = caam_decap_blob((ulong)buffer, (ulong)data, key_modifier, ENV_SIZE - BLOB_OVERHEAD);

	if (ret)
		goto err;

	memcpy(data, buffer, ENV_SIZE);

err:
	free(buffer);
	return ret;
}
#elif CONFIG_ENV_AES_CCMP1
int env_aes_cbc_crypt(env_t *env, const int enc)
{
	unsigned char *data = env->data;
	int ret = 0;
	/* get NAND geometrics */
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (enc) {
		crypt_cipher_data(TA_AES_MODE_ENCODE, data, mtd->erasesize);
	} else {
		crypt_cipher_data(TA_AES_MODE_DECODE, data, mtd->erasesize);
	}

	return ret;
}
#endif
