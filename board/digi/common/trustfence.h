/*
 *  Copyright (C) 2016 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef TRUSTFENCE_H
#define TRUSTFENCE_H

#ifdef CONFIG_HAS_TRUSTFENCE
#ifdef CONFIG_CONSOLE_ENABLE_GPIO
int console_enable_gpio(int gpio);
#endif
#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
int console_enable_passphrase(void);
#endif

int is_uboot_encrypted(void);
void migrate_filesystem_key(void);
void copy_dek(void);
#endif /* CONFIG_HAS_TRUSTFENCE */

#ifdef CONFIG_ENV_AES_CAAM_KEY
void fdt_fixup_trustfence(void *fdt);
#else
static inline void fdt_fixup_trustfence(void *fdt) {}
#endif

int get_trustfence_key_modifier(unsigned char key_modifier[16]);
#endif /* TRUSTFENCE_H */
