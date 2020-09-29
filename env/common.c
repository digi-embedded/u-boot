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
#include <fsl_sec.h>
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

/*
 * Read an environment variable as a boolean
 * Return -1 if variable does not exist (default to true)
 */
int env_get_yesno(const char *var)
{
	char *s = env_get(var);

	if (s == NULL)
		return -1;
	return (*s == '1' || *s == 'y' || *s == 'Y' || *s == 't' || *s == 'T') ?
		1 : 0;
}

/*
 * Look up the variable from the default environment
 */
char *env_get_default(const char *name)
{
	char *ret_val;
	unsigned long really_valid = gd->env_valid;
	unsigned long real_gd_flags = gd->flags;

	/* Pretend that the image is bad. */
	gd->flags &= ~GD_FLG_ENV_READY;
	gd->env_valid = ENV_INVALID;
	ret_val = env_get(name);
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
			flags = H_INTERACTIVE;
			puts(s);
		}
	} else {
		debug("Using default environment\n");
	}

	if (himport_r(&env_htab, (char *)default_environment,
			sizeof(default_environment), '\0', flags, 0,
			0, NULL) == 0)
		pr_err("Environment import failed: errno = %d\n", errno);

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
#include <asm/mach-imx/hab.h>
#include "../board/digi/common/trustfence.h"

static int env_aes_cbc_crypt(env_t *env, const int enc)
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
		ret = caam_gen_blob((uintptr_t)data, (uintptr_t)buffer, key_modifier, ENV_SIZE - BLOB_OVERHEAD);
	else
		ret = caam_decap_blob((uintptr_t)buffer, (uintptr_t)data, key_modifier, ENV_SIZE - BLOB_OVERHEAD);

	if (ret)
		goto err;

	memcpy(data, buffer, ENV_SIZE);

err:
	free(buffer);
	return ret;
}
#endif /* CONFIG_ENV_AES_CAAM_KEY */

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
			return -EIO;
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
			return 0;
		}
#endif
		pr_err("Failed to decrypt env!\n");
		set_default_env("!import failed");
		return ret;
	} else {
		if (himport_r(&env_htab, (char *)ep->data, ENV_SIZE, '\0', 0, 0,
			0, NULL)) {
			gd->flags |= GD_FLG_ENV_READY;
			return 0;
		}
	}

	pr_err("Cannot import environment: errno = %d\n", errno);

	set_default_env("!import failed");

	return -EIO;
}

#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
static unsigned char env_flags;

int env_import_redund(const char *buf1, int buf1_read_fail,
		      const char *buf2, int buf2_read_fail)
{
	int crc1_ok, crc2_ok;
	env_t *ep, *tmp_env1, *tmp_env2;

	tmp_env1 = (env_t *)buf1;
	tmp_env2 = (env_t *)buf2;

	if (buf1_read_fail && buf2_read_fail) {
		puts("*** Error - No Valid Environment Area found\n");
	} else if (buf1_read_fail || buf2_read_fail) {
		puts("*** Warning - some problems detected ");
		puts("reading environment; recovered successfully\n");
	}

	if (buf1_read_fail && buf2_read_fail) {
		set_default_env("!bad env area");
		return -EIO;
	} else if (!buf1_read_fail && buf2_read_fail) {
		gd->env_valid = ENV_VALID;
		return env_import((char *)tmp_env1, 1);
	} else if (buf1_read_fail && !buf2_read_fail) {
		gd->env_valid = ENV_REDUND;
		return env_import((char *)tmp_env2, 1);
	}

	crc1_ok = crc32(0, tmp_env1->data, ENV_SIZE) ==
			tmp_env1->crc;
	crc2_ok = crc32(0, tmp_env2->data, ENV_SIZE) ==
			tmp_env2->crc;

	if (!crc1_ok && !crc2_ok) {
		set_default_env("!bad CRC");
		return -EIO;
	} else if (crc1_ok && !crc2_ok) {
		gd->env_valid = ENV_VALID;
	} else if (!crc1_ok && crc2_ok) {
		gd->env_valid = ENV_REDUND;
	} else {
		/* both ok - check serial */
		if (tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = ENV_REDUND;
		else if (tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = ENV_VALID;
		else if (tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = ENV_VALID;
		else if (tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = ENV_REDUND;
		else /* flags are equal - almost impossible */
			gd->env_valid = ENV_VALID;
	}

	if (gd->env_valid == ENV_VALID)
		ep = tmp_env1;
	else
		ep = tmp_env2;

	env_flags = ep->flags;
	return env_import((char *)ep, 0);
}
#endif /* CONFIG_SYS_REDUNDAND_ENVIRONMENT */

/* Export the environment and generate CRC for it. */
int env_export(env_t *env_out)
{
	char *res;
	ssize_t	len;
#ifdef CONFIG_ENV_AES_CAAM_KEY
	int ret;
#endif

	res = (char *)env_out->data;
	len = hexport_r(&env_htab, '\0', 0, &res, ENV_SIZE, 0, NULL);
	if (len < 0) {
		pr_err("Cannot export environment: errno = %d\n", errno);
		return 1;
	}

	/* Encrypt the env if desired. */
#ifdef CONFIG_ENV_AES_CAAM_KEY
	ret = env_aes_cbc_crypt(env_out, 1);
	if (ret)
		return ret;
#endif

	env_out->crc = crc32(0, env_out->data, ENV_SIZE);

#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
	env_out->flags = ++env_flags; /* increase the serial */
#endif

	return 0;
}

void env_relocate(void)
{
#if defined(CONFIG_NEEDS_MANUAL_RELOC)
	env_reloc();
	env_htab.change_ok += gd->reloc_off;
#endif
	if (gd->env_valid == ENV_INVALID) {
#if defined(CONFIG_ENV_IS_NOWHERE) || defined(CONFIG_ENV_DEFAULT_NOWHERE) || defined(CONFIG_SPL_BUILD)
		/* Environment not changable */
		set_default_env(NULL);
#else
		bootstage_error(BOOTSTAGE_ID_NET_CHECKSUM);
		set_default_env("!bad CRC");
#endif
	} else {
		env_load();
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
