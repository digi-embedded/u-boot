/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <asm/mach-imx/hab.h>
#include <env_internal.h>
#include <fsl_sec.h>
#include <fuse.h>
#include <memalign.h>
#include <u-boot/md5.h>
#if defined(CONFIG_ARCH_MX6) || defined(CONFIG_ARCH_MX7) || \
	defined(CONFIG_ARCH_MX7ULP) || defined(CONFIG_ARCH_IMX8M)
#include <asm/arch/clock.h>
#endif

static int get_trustfence_key_modifier(unsigned char key_modifier[16])
{
	u32 ocotp_hwid[CONFIG_HWID_WORDS_NUMBER];
	int i, ret;

	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		ret = fuse_read(CONFIG_HWID_BANK,
				CONFIG_HWID_START_WORD + i, &ocotp_hwid[i]);
		if (ret)
			return ret;
	}
	md5((unsigned char *)(&ocotp_hwid), sizeof(ocotp_hwid), key_modifier);

	return ret;
}

int env_aes_cbc_crypt(env_t * env, const int enc)
{
	unsigned char *data = env->data;
	int ret = 0;
	uint8_t *src_ptr, *dst_ptr, *key_mod;

	if (!imx_hab_is_enabled())
		return 0;

	/* Buffers must be aligned */
	key_mod = memalign(ARCH_DMA_MINALIGN, KEY_MODIFER_SIZE);
	if (!key_mod) {
		debug("Not enough memory to encrypt the environment\n");
		return -ENOMEM;
	}
	ret = get_trustfence_key_modifier(key_mod);
	if (ret)
		goto freekm;

	src_ptr = memalign(ARCH_DMA_MINALIGN, ENV_SIZE);
	if (!src_ptr) {
		debug("Not enough memory to encrypt the environment\n");
		ret = -ENOMEM;
		goto freekm;
	}
	dst_ptr = memalign(ARCH_DMA_MINALIGN, ENV_SIZE);
	if (!dst_ptr) {
		debug("Not enough memory to encrypt the environment\n");
		ret = -ENOMEM;
		goto freesrc;
	}
	memcpy(src_ptr, data, ENV_SIZE);

#if defined(CONFIG_ARCH_MX6) || defined(CONFIG_ARCH_MX7) || \
	defined(CONFIG_ARCH_MX7ULP) || defined(CONFIG_ARCH_IMX8M)
	hab_caam_clock_enable(1);
	u32 out_jr_size = sec_in32(CFG_SYS_FSL_JR0_ADDR + FSL_CAAM_ORSR_JRa_OFFSET);
	if (out_jr_size != FSL_CAAM_MAX_JR_SIZE)
		sec_init();
#endif

	if (enc)
		ret = blob_encap(key_mod, src_ptr, dst_ptr, ENV_SIZE - BLOB_OVERHEAD, 0);
	else
		ret = blob_decap(key_mod, src_ptr, dst_ptr, ENV_SIZE - BLOB_OVERHEAD, 0);

	if (ret)
		goto err;

	memcpy(data, dst_ptr, ENV_SIZE);

err:
	free(dst_ptr);
freesrc:
	free(src_ptr);
freekm:
	free(key_mod);

	return ret;
}
