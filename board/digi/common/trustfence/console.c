// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024-2026 Digi International Inc.
 */

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
#include <asm-generic/gpio.h>
#endif
#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
#include <cyclic.h>
#include <log.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <u-boot/sha256.h>
#include "../helper.h"
#endif

#include "console.h"

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
/*
 * Enable console when selected GPIO level is HIGH
 * @name: Name of the GPIO to read
 *
 * Returns the GPIO level on success and 0 on error.
 */
int console_enable_gpio(const char *name)
{
	struct gpio_desc desc;
	ulong flags = GPIOD_IS_IN;
	int ret = 0;

	if (dm_gpio_lookup_name(name, &desc))
		goto error;

	if (dm_gpio_request(&desc, "Console enable"))
		goto error;

	if (IS_ENABLED(CONFIG_CONSOLE_ENABLE_GPIO_ACTIVE_LOW))
		flags |= GPIOD_ACTIVE_LOW;

	if (dm_gpio_set_dir_flags(&desc, flags))
		goto errfree;

	ret = dm_gpio_get_value(&desc);
errfree:
	dm_gpio_free(NULL, &desc);
error:
	return ret;
}
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */

#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
#define INACTIVITY_TIMEOUT		2

/*
 * Grab a passphrase from the console input
 * @secs: Inactivity timeout in seconds
 * @buff: Pointer to passphrase output buffer
 * @len: Length of output buffer
 *
 * Returns zero on success and a negative number on error.
 */
static int console_get_passphrase(int secs, char *buff, int len)
{
	char c;
	uint64_t end_tick, timeout;
	int i;

	/* Set a global timeout to avoid DoS attacks */
	end_tick = get_ticks() + (uint64_t)(secs * get_tbclk());

	/* Set an inactivity timeout */
	timeout = get_ticks() + INACTIVITY_TIMEOUT * get_tbclk();

	*buff = '\0';
	for (i = 0; i < len;) {
		/* Check timeouts */
		uint64_t tks = get_ticks();

		if ((tks > end_tick) || (tks > timeout)) {
			*buff = '\0';
			return -ETIME;
		}

		/* Do not trigger watchdog while typing passphrase */
		schedule();

		if (tstc()) {
			c = getchar();
			i++;
		} else {
			continue;
		}

		switch (c) {
		/* Enter */
		case '\r':
		case '\n':
			*buff = '\0';
			return 0;
		/* nul */
		case '\0':
			continue;
		/* Ctrl-c */
		case 0x03:
			*buff = '\0';
			return -EINVAL;
		default:
			*buff++ = c;
			/* Restart inactivity timeout */
			timeout = get_ticks() + INACTIVITY_TIMEOUT *
				get_tbclk();
		}
	}

	return -EINVAL;
}

#define SHA256_HASH_LEN		32
#define PASSPHRASE_SECS_TIMEOUT	10
#define MAX_PP_LEN			64

/*
 * Returns zero (success) to enable the console, or a non zero otherwise.
 *
 * A sha256 hash is 256 bits (32 bytes) long, and is represented as
 * a 64 digits hex number.
 */
int console_enable_passphrase(void)
{
	char *pp = NULL;
	unsigned char *sha256_pp = NULL;
	unsigned char *pp_hash = NULL;
	int ret = -EINVAL;

	pp = malloc(MAX_PP_LEN + 1);
	if (!pp) {
		debug("Not enough memory for passphrase\n");
		return -ENOMEM;
	}

	ret = console_get_passphrase(PASSPHRASE_SECS_TIMEOUT,
				     pp, MAX_PP_LEN);
	if (ret)
		goto pp_error;

	sha256_pp = malloc(SHA256_HASH_LEN);
	if (!sha256_pp) {
		debug("Not enough memory for passphrase\n");
		ret = -ENOMEM;
		goto pp_error;
	}

	sha256_csum_wd((const unsigned char *)pp, strlen(pp),
		       sha256_pp, CHUNKSZ_SHA256);

	pp_hash = malloc(SHA256_HASH_LEN);
	if (!pp_hash) {
		debug("Not enough memory for passphrase\n");
		ret = -ENOMEM;
		goto pp_hash_error;
	}

	memset(pp_hash, 0, SHA256_HASH_LEN);
	strtohex(CONFIG_CONSOLE_ENABLE_PASSPHRASE_KEY, pp_hash,
		 SHA256_HASH_LEN);
	ret = memcmp(sha256_pp, pp_hash, SHA256_HASH_LEN);

	free(pp_hash);
pp_hash_error:
	free(sha256_pp);
pp_error:
	free(pp);
	return ret;
}
#endif /* CONFIG_CONSOLE_ENABLE_PASSPHRASE */
