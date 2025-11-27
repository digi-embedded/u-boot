// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2023-2025 Digi International Inc - All Rights Reserved
 */

#include <common.h>
#include <command.h>
#include <fdt_support.h>
#include <fuse.h>
#include <linux/errno.h>
#include <u-boot/sha256.h>
#include <watchdog.h>
#include <malloc.h>
#include <mapmem.h>
#include <memalign.h>
#include <misc.h>
#include <asm/arch/bsec.h>
#include <console.h>

#include "../board/digi/common/helper.h"
#include "../board/digi/common/trustfence.h"

/*
 * NOTE: This is a copy from cmd_stm32key.c
 *       It requires to be updated from the original!
 */
struct stm32key {
	char *name;
	char *desc;
	u16 start;
	u8 size;
	int (*post_process)(struct udevice *dev, const struct stm32key *key);
};

#define STM32_OTP_MODE_WORD			0
#define STM32_OTP_STM32MP13x_OPEN_MASK		0x17
#define STM32_OTP_STM32MP13x_CLOSE_MASK		0x3F
#define STM32_OTP_STM32MP13x_BSCANDIS_MASK	0x17F
#define STM32_OTP_STM32MP13x_JTAGDIS_MASK	0x3FF
#define STM32_OTP_STM32MP15x_CLOSE_MASK		BIT(6)

#ifdef CONFIG_STM32MP13X
#define PKHTH_KEY_NAME		"PKHTH"
#define EDMK_KEY_NAME		"EDMK"
#endif /* CONFIG_STM32MP13X */

#ifdef CONFIG_STM32MP15X
#define PKH_KEY_NAME		"PKH"
#endif /* CONFIG_STM32MP15X */

#ifdef CONFIG_STM32MP25X
#define PKHTH_KEY_NAME		"OEM-KEY1"
#define PKHTH2_KEY_NAME		"OEM-KEY2"
#define RPROC_PKH_KEY_NAME	"RPROC-FW-PKH"
#define EDMK_KEY_NAME		"EDMK1"
#define EDMK2_KEY_NAME		"EDMK2"
#define FIP_EDMK_KEY_NAME	"FIP-EDMK"
#define RPROC_EDMK_KEY_NAME	"RPROC-FW-ENC-KEY"
#endif /* CONFIG_STM32MP25X */

enum jtag_status {
	JTAG_OPEN,
	BSCAN_DISABLED,
	JTAG_DISABLED,
};

char *jtag_desc[] = {
	"[OPEN]",
	"[BSCAN disabled]",
	"[DISABLED]",
};

enum tf_op {
	TF_READ,
	TF_PROG
};

struct tf_cmd {
	const char *subcmd;
	const char *key_name;
	enum tf_op op;
};

static const struct tf_cmd tf_cmds[] = {
#ifdef CONFIG_STM32MP15X
	{ "read_pkh",        PKH_KEY_NAME,        TF_READ },
	{ "prog_pkh",        PKH_KEY_NAME,        TF_PROG },
#else /* CONFIG_STM32MP13X || CONFIG_STM32MP25X */
	{ "read_pkhth",      PKHTH_KEY_NAME,      TF_READ },
	{ "prog_pkhth",      PKHTH_KEY_NAME,      TF_PROG },
	{ "read_edmk",       EDMK_KEY_NAME,       TF_READ },
	{ "prog_edmk",       EDMK_KEY_NAME,       TF_PROG },
#ifdef CONFIG_STM32MP25X
	{ "read_fip_edmk",   FIP_EDMK_KEY_NAME,   TF_READ },
	{ "prog_fip_edmk",   FIP_EDMK_KEY_NAME,   TF_PROG },
	{ "read_pkhth2",     PKHTH2_KEY_NAME,     TF_READ },
	{ "prog_pkhth2",     PKHTH2_KEY_NAME,     TF_PROG },
	{ "read_edmk2",      EDMK2_KEY_NAME,      TF_READ },
	{ "prog_edmk2",      EDMK2_KEY_NAME,      TF_PROG },
	{ "read_rproc_pkh",  RPROC_PKH_KEY_NAME,  TF_READ },
	{ "prog_rproc_pkh",  RPROC_PKH_KEY_NAME,  TF_PROG },
	{ "read_rproc_edmk", RPROC_EDMK_KEY_NAME, TF_READ },
	{ "prog_rproc_edmk", RPROC_EDMK_KEY_NAME, TF_PROG },
#endif
#endif
};

struct tf_key {
    const char *name;
    bool readable;
};

static const struct tf_key tf_keys[] = {
#ifdef CONFIG_STM32MP15X
	{ PKH_KEY_NAME,        true },
#else /* CONFIG_STM32MP13X || CONFIG_STM32MP25X */
	{ PKHTH_KEY_NAME,      true },
	{ EDMK_KEY_NAME,       false },
#ifdef CONFIG_STM32MP25X
	{ FIP_EDMK_KEY_NAME,   false },
	{ PKHTH2_KEY_NAME,     true },
	{ EDMK2_KEY_NAME,      false },
	{ RPROC_PKH_KEY_NAME,  true },
	{ RPROC_EDMK_KEY_NAME, false },
#endif
#endif
};

/* Functions defined in cmd_stm32key.c */
extern int get_misc_dev(struct udevice **dev);
extern const struct stm32key *get_key(u8 index);
extern u8 get_key_nb(void);
extern int read_key_otp(struct udevice *dev, const struct stm32key *key, bool print, bool *locked);
extern void read_key_value(const struct stm32key *key, u32 addr);
extern int fuse_key_value(struct udevice *dev, const struct stm32key *key, u32 addr, bool print);
extern int do_stm32key_close(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]);

static int get_key_index(char *const key_name)
{
	const struct stm32key *key;
	int i;

	if (!key_name) {
		printf("Error: key name is NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < get_key_nb(); i++) {
		key = get_key(i);
		if (!strcmp(key->name, key_name)) {
			return i;
		}
	}

	printf("Error: can't find key %s\n", key_name);
	return -1;
}

static bool is_key_readable(int key_index)
{
	const struct stm32key *key = get_key(key_index);

	if (!key || !key->name)
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(tf_keys); ++i) {
		if (strcmp(tf_keys[i].name, key->name) == 0)
			return tf_keys[i].readable;
	}

	printf("Error: can't find key index %d\n", key_index);
	return false;
}

static void read_key_otp_stat(struct udevice *dev, int key_index)
{
	const struct stm32key *key = get_key(key_index);
	int nb_invalid = 0, nb_zero = 0, nb_lock = 0, nb_lock_err = 0;
	int i, word, ret;
	u32 val, lock;
	bool status;
	char prefix[64];

	for (i = 0, word = key->start; i < key->size; i++, word++) {
		ret = misc_read(dev, STM32_BSEC_OTP(word), &val, 4);
		if (ret != 4)
			val = ~0x0;
		ret = misc_read(dev, STM32_BSEC_LOCK(word), &lock, 4);
		if (ret != 4)
			lock = BSEC_LOCK_ERROR;
		if (val == ~0x0)
			nb_invalid++;
		else if (val == 0x0)
			nb_zero++;
		if (lock & BSEC_LOCK_PERM)
			nb_lock++;
		if (lock & BSEC_LOCK_ERROR)
			nb_lock_err++;
	}

	snprintf(prefix, sizeof(prefix), "* %s fuses:", key->name);
	printf("%-25s ", prefix);
	if (nb_invalid == key->size)
		printf("[INVALID] ");
	else if (nb_zero == key->size)
		printf("[NOT PROGRAMMED] ");
	else
		printf("[PROGRAMMED] ");
	status = nb_lock_err || (nb_lock == key->size);
	if (nb_lock_err)
		printf("[INVALID LOCK STATUS]");
	else if (status)
		printf("[LOCKED]");
	else
		printf("[NOT LOCKED]");
	printf("\n");

}

static int trustfence_read_key(int argc, char *const argv[], int key_index)
{
	const struct stm32key *key = get_key(key_index);
	struct udevice *dev;
	u32 addr;
	int ret;

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	if (argc == 1) {
		if (ret)
			return CMD_RET_FAILURE;
		ret = read_key_otp(dev, key, true, NULL);
		if (ret != -ENOENT)
			return CMD_RET_FAILURE;
		return CMD_RET_SUCCESS;
	}

	addr = hextoul(argv[1], NULL);
	if (!addr)
		return CMD_RET_USAGE;

	printf("Read %s at 0x%08x\n", key->name, addr);
	read_key_value(key, addr);

	return CMD_RET_SUCCESS;
}

static int trustfence_prog_key(int argc, char *const argv[], int key_index)
{
	const struct stm32key *key = get_key(key_index);
	struct udevice *dev;
	u32 addr;
	int ret;
	bool yes = false, lock;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (argc == 3) {
		if (strcmp(argv[1], "-y"))
			return CMD_RET_USAGE;
		yes = true;
	}

	addr = hextoul(argv[argc - 1], NULL);
	if (!addr)
		return CMD_RET_USAGE;

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	if (read_key_otp(dev, key, !yes, &lock) != -ENOENT) {
		printf("Error: can't fuse again the OTP\n");
		return CMD_RET_FAILURE;
	}
	if (lock) {
		printf("Error: %s is locked\n", key->name);
		return CMD_RET_FAILURE;
	}

	if (!yes) {
		printf("Writing %s with\n", key->name);
		read_key_value(key, addr);
	}

	if (!yes && !confirm_prog())
		return CMD_RET_FAILURE;

	if (fuse_key_value(dev, key, addr, !yes))
		return CMD_RET_FAILURE;

	printf("%s updated !\n", key->name);

	return CMD_RET_SUCCESS;
}

static int read_otp_mode(struct udevice *dev, bool *closed, int *jtag)
{
	int ret;
	u32 val;

	/* Read OTP mode (for close and JTAG status) */
	ret = misc_read(dev, STM32_BSEC_OTP(STM32_OTP_MODE_WORD), &val, 4);
	if (ret < 0) {
		printf("Error: can't read OTP mode\n");
		return -1;
	}

	if (IS_ENABLED(CONFIG_STM32MP15X)) {
		*closed = (val & STM32_OTP_STM32MP15x_CLOSE_MASK) ==
			  STM32_OTP_STM32MP15x_CLOSE_MASK;
		*jtag = *closed ? JTAG_DISABLED : JTAG_OPEN;
	}
	if (IS_ENABLED(CONFIG_STM32MP13X)) {
		*closed = (val & STM32_OTP_STM32MP13x_CLOSE_MASK) ==
			  STM32_OTP_STM32MP13x_CLOSE_MASK;
		switch(val) {
		case STM32_OTP_STM32MP13x_OPEN_MASK:
		case STM32_OTP_STM32MP13x_CLOSE_MASK:
			*jtag = JTAG_OPEN;
			break;
		case STM32_OTP_STM32MP13x_BSCANDIS_MASK:
			*jtag = BSCAN_DISABLED;
			break;
		case STM32_OTP_STM32MP13x_JTAGDIS_MASK:
			*jtag = JTAG_DISABLED;
			break;
		default:
			printf("Error: invalid OTP mode\n");
			return -1;
		}
	}

	return 0;
}

static const struct tf_cmd *find_tf_cmd(const char *name)
{
	for (size_t i = 0; i < ARRAY_SIZE(tf_cmds); ++i)
		if (!strcmp(tf_cmds[i].subcmd, name))
			return &tf_cmds[i];
	return NULL;
}

static int do_trustfence_key_dispatch(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const struct tf_cmd *c = find_tf_cmd(cmdtp->name);
	if (!c)
		return CMD_RET_USAGE;

	int key_index = get_key_index((char *)c->key_name);
	if (key_index < 0)
		return CMD_RET_FAILURE;

	switch (c->op) {
		case TF_READ:
			return trustfence_read_key(argc, argv, key_index);

		case TF_PROG:
			return trustfence_prog_key(argc, argv, key_index);

		default:
			return CMD_RET_FAILURE;
	}
}

#ifdef CONFIG_STM32MP13X
static int do_trustfence_prog_jtag(struct cmd_tbl *cmdtp, int flag, int argc,
				   char *const argv[])
{
	struct udevice *dev;
	bool closed;
	int jtag, newmode;
	bool yes = false;
	u32 val = 0;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (argc == 3) {
		if (strcmp(argv[1], "-y"))
			return CMD_RET_USAGE;
		yes = true;
	}

	if (!strcmp(argv[argc - 1], "disable-bscan")) {
		newmode = BSCAN_DISABLED;
		val = STM32_OTP_STM32MP13x_BSCANDIS_MASK;
	} else if (!strcmp(argv[argc - 1], "disable-jtag")) {
		newmode = JTAG_DISABLED;
		val = STM32_OTP_STM32MP13x_JTAGDIS_MASK;
	} else {
		return CMD_RET_USAGE;
	}

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	/* Read OTP mode (for close and JTAG status) */
	ret = read_otp_mode(dev, &closed, &jtag);
	if (ret < 0)
		return CMD_RET_FAILURE;

	/* Abort if the device is not closed or mode already programmed */
	if (!closed) {
		printf("Device secure boot status is OPEN.\n");
		printf("The JTAG port can only be secured on closed devices\n");
		return CMD_RET_FAILURE;
	} else if (newmode <= jtag) {
		printf("Current JTAG port status is already %s.\n",
		       jtag_desc[jtag]);
		return CMD_RET_FAILURE;
	}

	if (!yes && !confirm_prog())
		return CMD_RET_FAILURE;

	ret = misc_write(dev, STM32_BSEC_OTP(STM32_OTP_MODE_WORD), &val, 4);
	if (ret != 4) {
		printf("Error: can't update OTP %d\n", STM32_OTP_MODE_WORD);
		return CMD_RET_FAILURE;
	}
	printf("Secure JTAG programmed!\n");

	return CMD_RET_SUCCESS;
}
#endif /* CONFIG_STM32MP13X */

static int do_trustfence_status(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct udevice *dev;
	int i, jtag, ret;
	bool closed;

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	for (i = 0; i < get_key_nb(); i++) {
		if (is_key_readable(i))
			read_key_otp_stat(dev, i);
	}

	/* Read OTP mode (for close and JTAG status) */
	ret = read_otp_mode(dev, &closed, &jtag);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("* %-23s %s\n", "Secure boot:", closed ? "[CLOSED]" : "[OPEN]");
	printf("* %-23s %s\n", "JTAG:", jtag_desc[jtag]);

	return CMD_RET_SUCCESS;
}

static int do_trustfence_close(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return do_stm32key_close(cmdtp, flag, argc, argv);
}

U_BOOT_CMD_WITH_SUBCMDS(trustfence, "Digi TrustFence(TM) command",
	"status - show secure boot configuration status\n"
#ifdef CONFIG_STM32MP15X
	"trustfence read_pkh [<addr>] - read Public Key Hash ("PKH_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_pkh [-y] <addr> <size in bytes> - burn Public Key Hash ("PKH_KEY_NAME") (PERMANENT)\n"
#else
	"trustfence read_pkhth [<addr>] - read Public Key Hash Table Hash ("PKHTH_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_pkhth [-y] <addr> - burn Public Key Hash Table Hash ("PKHTH_KEY_NAME") (PERMANENT)\n"
	"trustfence read_edmk [<addr>] - read Encryption Decryption master key ("EDMK_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_edmk [-y] <addr> - burn Encryption Decryption master key ("EDMK_KEY_NAME") (PERMANENT)\n"
#ifdef CONFIG_STM32MP25X
	"trustfence read_fip_edmk [<addr>] - read Encryption Decryption master key for FIP ("FIP_EDMK_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_fip_edmk [-y] <addr> - burn Encryption Decryption master key for FIP ("FIP_EDMK_KEY_NAME") (PERMANENT)\n"
	"trustfence read_pkhth2 [<addr>] - read Public Key Hash Table Hash for FSBLM ("PKHTH2_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_pkhth2 [-y] <addr> - burn Public Key Hash Table Hash for FSBLM ("PKHTH2_KEY_NAME") (PERMANENT)\n"
	"trustfence read_edmk2 [<addr>] - read Encryption Decryption master key for FSBLM ("EDMK2_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_edmk2 [-y] <addr> - burn Encryption Decryption master key for FSBLM ("EDMK2_KEY_NAME") (PERMANENT)\n"
	"trustfence read_rproc_pkh [<addr>] - read Public Key Hash for remote processor ("RPROC_PKH_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_rproc_pkh [-y] <addr> <size in bytes> - burn Public Key Hash for remote processor ("RPROC_PKH_KEY_NAME") (PERMANENT)\n"
	"trustfence read_rproc_edmk [<addr>] - read Encryption Decryption master key for remote processor ("RPROC_EDMK_KEY_NAME") at <addr> or all key in OTP\n"
	"trustfence prog_rproc_edmk [-y] <addr> - burn Encryption Decryption master key for remote processor ("RPROC_EDMK_KEY_NAME") (PERMANENT)\n"
#endif /* CONFIG_STM32MP25X */
#ifdef CONFIG_STM32MP13X
	"trustfence prog_jtag [-y] <mode> - program Secure JTAG mode <mode> (PERMANENT). <mode> can be one of:\n"
	"    disable-bscan - Boundary scan disabled\n"
	"    disable-jtag  - JTAG disabled\n"
#endif /* CONFIG_STM32MP13X */
#endif /* CONFIG_STM32MP15X */
	"trustfence close [-y] - close the device so that it can only boot signed images (PERMANENT)\n",
	U_BOOT_SUBCMD_MKENT(status, 1, 0, do_trustfence_status),
#ifdef CONFIG_STM32MP15X
	U_BOOT_SUBCMD_MKENT(read_pkh, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_pkh, 3, 0, do_trustfence_key_dispatch),
#else
	U_BOOT_SUBCMD_MKENT(read_pkhth, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_pkhth, 3, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(read_edmk, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_edmk, 3, 0, do_trustfence_key_dispatch),
#ifdef CONFIG_STM32MP25X
	U_BOOT_SUBCMD_MKENT(read_fip_edmk, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_fip_edmk, 3, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(read_pkhth2, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_pkhth2, 3, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(read_edmk2, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_edmk2, 3, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(read_rproc_pkh, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_rproc_pkh, 3, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(read_rproc_edmk, 2, 0, do_trustfence_key_dispatch),
	U_BOOT_SUBCMD_MKENT(prog_rproc_edmk, 3, 0, do_trustfence_key_dispatch),
#endif /* CONFIG_STM32MP25X */
#ifdef CONFIG_STM32MP13X
	U_BOOT_SUBCMD_MKENT(prog_jtag, 3, 0, do_trustfence_prog_jtag),
#endif /* CONFIG_STM32MP13X */
#endif /* CONFIG_STM32MP15X */
	U_BOOT_SUBCMD_MKENT(close, 2, 0, do_trustfence_close)
);
