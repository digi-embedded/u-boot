/*
 * Copyright (C) 2024-2026, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
#include <asm/gpio.h>
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
