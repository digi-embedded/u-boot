/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <search.h>
#include <errno.h>
#include <malloc.h>

DECLARE_GLOBAL_DATA_PTR;

/************************************************************************
 * Default settings to be used when no valid environment is found
 */
#include <env_default.h>

struct hsearch_data env_htab = {
	.change_ok = env_flags_validate,
};

#if defined(CONFIG_CMD_NAND) && defined(CONFIG_CMD_BSP)
size_t nvram_part_size;
#endif

__weak uchar env_get_char_spec(int index)
{
	return *((uchar *)(gd->env_addr + index));
}

static uchar env_get_char_init(int index)
{
	/* if crc was bad, use the default environment */
	if (gd->env_valid)
		return env_get_char_spec(index);
	else
		return default_environment[index];
}

uchar env_get_char_memory(int index)
{
	return *env_get_addr(index);
}

uchar env_get_char(int index)
{
	/* if relocated to RAM */
	if (gd->flags & GD_FLG_RELOC)
		return env_get_char_memory(index);
	else
		return env_get_char_init(index);
}

const uchar *env_get_addr(int index)
{
	if (gd->env_valid)
		return (uchar *)(gd->env_addr + index);
	else
		return &default_environment[index];
}

/*
 * Read an environment variable as a boolean
 * Return -1 if variable does not exist (default to true)
 */
int getenv_yesno(const char *var)
{
	char *s = getenv(var);

	if (s == NULL)
		return -1;
	return (*s == '1' || *s == 'y' || *s == 'Y' || *s == 't' || *s == 'T') ?
		1 : 0;
}

/*
 * Look up the variable from the default environment
 */
char *getenv_default(const char *name)
{
	char *ret_val;
	unsigned long really_valid = gd->env_valid;
	unsigned long real_gd_flags = gd->flags;

	/* Pretend that the image is bad. */
	gd->flags &= ~GD_FLG_ENV_READY;
	gd->env_valid = 0;
	ret_val = getenv(name);
	gd->env_valid = really_valid;
	gd->flags = real_gd_flags;
	return ret_val;
}

__weak void platform_default_environment(void)
{
	return;
}

void set_default_env(const char *s)
{
	int flags = 0;

	if (sizeof(default_environment) > ENV_SIZE) {
		puts("*** Error - default environment is too large\n\n");
		return;
	}

	if (s) {
		if (*s == '!') {
			printf("*** Warning - %s, "
				"using default environment\n\n",
				s + 1);
		} else {
			if (*s == '*')
				flags |= H_FORCE;	/* Forced reset */
			flags |= H_INTERACTIVE;
			puts(s);
		}
	} else {
		puts("Using default environment\n\n");
	}

	if (himport_r(&env_htab, (char *)default_environment,
			sizeof(default_environment), '\0', flags, 0,
			0, NULL) == 0)
		error("Environment import failed: errno = %d\n", errno);

	/* Platform-specific actions on default environment */
	platform_default_environment();

	gd->flags |= GD_FLG_ENV_READY;
	gd->flags |= GD_FLG_ENV_DEFAULT;
}


/* [re]set individual variables to their value in the default environment */
int set_default_vars(int nvars, char * const vars[], int flag)
{
	/*
	 * Special use-case: import from default environment
	 * (and use \0 as a separator)
	 */
	return himport_r(&env_htab, (const char *)default_environment,
				sizeof(default_environment), '\0',
				flag | H_NOCLEAR | H_INTERACTIVE, 0, nvars, vars);
}

#ifdef CONFIG_ENV_AES

#ifdef CONFIG_ENV_AES_CAAM_KEY
#include <fuse.h>
#include <fsl_caam.h>
#include <u-boot/md5.h>
#include <asm/imx-common/hab.h>
#include "../board/digi/common/trustfence.h"

static int env_aes_cbc_crypt(env_t *env, const int enc)
{
	unsigned char *data = env->data;
	unsigned char *buffer;
	int ret = 0;
	unsigned char key_modifier[16] = {0};

	if (!is_hab_enabled())
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
		ret = caam_gen_blob(data, buffer, key_modifier, ENV_SIZE - BLOB_OVERHEAD);
	else
		ret = caam_decap_blob(buffer, data, key_modifier, ENV_SIZE - BLOB_OVERHEAD);

	if (ret)
		goto err;
	
	memcpy(data, buffer, ENV_SIZE);
	
err:
	free(buffer);
	return ret;
}
#else

#include <aes.h>
/**
 * env_aes_cbc_get_key() - Get AES-128-CBC key for the environment
 *
 * This function shall return 16-byte array containing AES-128 key used
 * to encrypt and decrypt the environment. This function must be overridden
 * by the implementer as otherwise the environment encryption will not
 * work.
 */
__weak uint8_t *env_aes_cbc_get_key(void)
{
	return NULL;
}

static int env_aes_cbc_crypt(env_t *env, const int enc)
{
	unsigned char *data = env->data;
	uint8_t *key;
	uint8_t key_exp[AES_EXPAND_KEY_LENGTH];
	uint32_t aes_blocks;

	key = env_aes_cbc_get_key();
	if (!key)
		return -EINVAL;

	/* First we expand the key. */
	aes_expand_key(key, key_exp);

	/* Calculate the number of AES blocks to encrypt. */
	aes_blocks = ENV_SIZE / AES_KEY_LENGTH;

	if (enc)
		aes_cbc_encrypt_blocks(key_exp, data, data, aes_blocks);
	else
		aes_cbc_decrypt_blocks(key_exp, data, data, aes_blocks);

	return 0;
}
#endif

#else
static inline int env_aes_cbc_crypt(env_t *env, const int enc)
{
	return 0;
}
#endif

/*
 * Check if CRC is valid and (if yes) import the environment.
 * Note that "buf" may or may not be aligned.
 */
int env_import(const char *buf, int check)
{
	env_t *ep = (env_t *)buf;
	int ret;

	if (check) {
		uint32_t crc;

		memcpy(&crc, &ep->crc, sizeof(crc));

		if (crc32(0, ep->data, ENV_SIZE) != crc) {
			set_default_env("!bad CRC");
			return 0;
		}
	}

	/* Decrypt the env if desired. */
	ret = env_aes_cbc_crypt(ep, 0);
	if (ret) {
#ifdef CONFIG_ENV_AES_CAAM_KEY
		if (himport_r(&env_htab, (char *)ep->data, ENV_SIZE,
				'\0', 0, 0, 0, NULL)) {
			printf("Environment is unencrypted!\n");
			printf("Resetting to defaults (read-only variables like MAC addresses will be kept).\n");
			gd->flags |= GD_FLG_ENV_READY;
			run_command("env default -a", 0);
			return 1;
		}
#endif
		error("Failed to decrypt env!\n");
		set_default_env("!import failed");
		return ret;
	}

	if (himport_r(&env_htab, (char *)ep->data, ENV_SIZE, '\0', 0, 0,
			0, NULL)) {
		gd->flags |= GD_FLG_ENV_READY;
		return 1;
	}

	error("Cannot import environment: errno = %d\n", errno);

	set_default_env("!import failed");

	return 0;
}

/* Export the environment and generate CRC for it. */
int env_export(env_t *env_out)
{
	char *res;
	ssize_t	len;
	int ret;

	res = (char *)env_out->data;
	len = hexport_r(&env_htab, '\0', 0, &res, ENV_SIZE, 0, NULL);
	if (len < 0) {
		error("Cannot export environment: errno = %d\n", errno);
		return 1;
	}

	/* Encrypt the env if desired. */
	ret = env_aes_cbc_crypt(env_out, 1);
	if (ret)
		return ret;

	env_out->crc = crc32(0, env_out->data, ENV_SIZE);

	return 0;
}

void env_relocate(void)
{
#if defined(CONFIG_CMD_NAND) && defined(CONFIG_CMD_BSP)
	/* Dynamic calculation of NVRAM partition size. */
	nvram_part_size = MtdGetEraseSize(0, PART_UBOOT_SIZE);
	while (nvram_part_size < CONFIG_ENV_SIZE)
		nvram_part_size += MtdGetEraseSize(0, PART_UBOOT_SIZE + nvram_part_size);

	/* space for NVRAM backup */
	nvram_part_size += MtdGetEraseSize(0, PART_UBOOT_SIZE + nvram_part_size);
	while (nvram_part_size < 2 * CONFIG_ENV_SIZE)
		nvram_part_size += MtdGetEraseSize(0, PART_UBOOT_SIZE + nvram_part_size);

	/* double NVRAM partition size to handle bad blcoks */
	nvram_part_size += nvram_part_size;
#endif
#if defined(CONFIG_NEEDS_MANUAL_RELOC)
	env_reloc();
	env_htab.change_ok += gd->reloc_off;
#endif
	if (gd->env_valid == 0) {
#if defined(CONFIG_ENV_IS_NOWHERE) || defined(CONFIG_SPL_BUILD)
		/* Environment not changable */
		set_default_env(NULL);
#else
		bootstage_error(BOOTSTAGE_ID_NET_CHECKSUM);
		set_default_env("!bad CRC");
#endif
	} else {
		env_relocate_spec();
	}
}

#if defined(CONFIG_AUTO_COMPLETE) && !defined(CONFIG_SPL_BUILD)
int env_complete(char *var, int maxv, char *cmdv[], int bufsz, char *buf)
{
	ENTRY *match;
	int found, idx;

	idx = 0;
	found = 0;
	cmdv[0] = NULL;

	while ((idx = hmatch_r(var, idx, &match, &env_htab))) {
		int vallen = strlen(match->key) + 1;

		if (found >= maxv - 2 || bufsz < vallen)
			break;

		cmdv[found++] = buf;
		memcpy(buf, match->key, vallen);
		buf += vallen;
		bufsz -= vallen;
	}

	qsort(cmdv, found, sizeof(cmdv[0]), strcmp_compar);

	if (idx)
		cmdv[found++] = "...";

	cmdv[found] = NULL;
	return found;
}
#endif
