// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2023-2024 Digi International Inc - All Rights Reserved
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

struct stm32key {
	char *name;
	char *desc;
	u8 start;
	u8 size;
};

#define STM32_OTP_MODE_WORD			0
#define STM32_OTP_STM32MP13x_OPEN_MASK		0x17
#define STM32_OTP_STM32MP13x_CLOSE_MASK		0x3F
#define STM32_OTP_STM32MP13x_BSCANDIS_MASK	0x17F
#define STM32_OTP_STM32MP13x_JTAGDIS_MASK	0x3FF
#define STM32_OTP_STM32MP15x_CLOSE_MASK		BIT(6)
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

/* Functions defined in cmd_stm32key.c */
extern int get_misc_dev(struct udevice **dev);
extern const struct stm32key *get_key(u8 index);
extern int read_key_otp(struct udevice *dev, const struct stm32key *key, bool print, bool *locked);
extern void read_key_value(const struct stm32key *key, u32 addr);
extern int fuse_key_value(struct udevice *dev, const struct stm32key *key, u32 addr, bool print);
extern int do_stm32key_close(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]);

static void read_key_otp_stat(struct udevice *dev, int key_index)
{
	const struct stm32key *key = get_key(key_index);
	int nb_invalid = 0, nb_zero = 0, nb_lock = 0, nb_lock_err = 0;
	int i, word, ret;
	u32 val, lock;
	bool status;

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

	printf("* %s fuses:\t", key->name);
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

static int do_trustfence_read_pkh(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return trustfence_read_key(argc, argv, 0);	/* PKH */
}

static int do_trustfence_prog_pkh(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return trustfence_prog_key(argc, argv, 0);	/* PKH */
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

	if (IS_ENABLED(CONFIG_STM32MP15x)) {
		*closed = (val & STM32_OTP_STM32MP15x_CLOSE_MASK) ==
			  STM32_OTP_STM32MP15x_CLOSE_MASK;
		*jtag = *closed ? JTAG_DISABLED : JTAG_OPEN;
	}
	if (IS_ENABLED(CONFIG_STM32MP13x)) {
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

#ifndef CONFIG_STM32MP15x
static int do_trustfence_read_edmk(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return trustfence_read_key(argc, argv, 1);	/* EDMK */
}

static int do_trustfence_prog_edmk(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	// TODO: enable when encryption of bootloader fully supported */
	printf("Encryption of bootloader not yet supported. Aborting.\n");
	return 0;
	//return trustfence_prog_key(argc, argv, 1);	/* EDMK */
}

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
	} else if (newmode == jtag) {
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
#endif /* !CONFIG_STM32MP15x */

static int do_trustfence_status(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct udevice *dev;
	int ret;
	const struct stm32key *key;
	bool closed;
	int jtag;

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	key = get_key(0);
	read_key_otp_stat(dev, 0);

	if (!IS_ENABLED(CONFIG_STM32MP15x))
		read_key_otp_stat(dev, 1);

	/* Read OTP mode (for close and JTAG status) */
	ret = read_otp_mode(dev, &closed, &jtag);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("* Secure boot:\t%s\n", closed ? "[CLOSED]" : "[OPEN]");
	printf("* JTAG:       \t%s\n", jtag_desc[jtag]);

	return CMD_RET_SUCCESS;
}

static int do_trustfence_close(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return do_stm32key_close(cmdtp, flag, argc, argv);
}

U_BOOT_CMD_WITH_SUBCMDS(trustfence, "Digi TrustFence(TM) command",
	"status - show secure boot configuration status\n"
#ifdef CONFIG_STM32MP15x
	"trustfence read_pkh [<addr>] - read Public Key Hash (PKH) at <addr> or all key in OTP\n"
	"trustfence prog_pkh [-y] <addr> <size in bytes> - burn Public Key Hash (PKH) (PERMANENT)\n"
#else
	"trustfence read_pkhth [<addr>] - read Public Key Hash Table Hash (PKHTH) at <addr> or all key in OTP\n"
	"trustfence prog_pkhth [-y] <addr> - burn Public Key Hash Table Hash (PKHTH) (PERMANENT)\n"
	"trustfence read_edmk [<addr>] - read Encryption Decryption master key (EDMK) at <addr> or all key in OTP\n"
	"trustfence prog_edmk [-y] <addr> - burn Encryption Decryption master key (EDMK) (PERMANENT)\n"
	"trustfence prog_jtag [-y] <mode> - program Secure JTAG mode <mode> (PERMANENT). <mode> can be one of:\n"
	"    disable-bscan - Boundary scan disabled\n"
	"    disable-jtag  - JTAG disabled\n"
#endif /* CONFIG_STM32MP15x */
	"trustfence close [-y] - close the device so that it can only boot signed images (PERMANENT)\n",
	U_BOOT_SUBCMD_MKENT(status, 1, 0, do_trustfence_status),
#ifdef CONFIG_STM32MP15x
	U_BOOT_SUBCMD_MKENT(read_pkh, 2, 0, do_trustfence_read_pkh),
	U_BOOT_SUBCMD_MKENT(prog_pkh, 3, 0, do_trustfence_prog_pkh),
#else
	U_BOOT_SUBCMD_MKENT(read_pkhth, 2, 0, do_trustfence_read_pkh),
	U_BOOT_SUBCMD_MKENT(prog_pkhth, 3, 0, do_trustfence_prog_pkh),
	U_BOOT_SUBCMD_MKENT(read_edmk, 2, 0, do_trustfence_read_edmk),
	U_BOOT_SUBCMD_MKENT(prog_edmk, 3, 0, do_trustfence_prog_edmk),
	U_BOOT_SUBCMD_MKENT(prog_jtag, 2, 0, do_trustfence_prog_jtag),
#endif /* CONFIG_STM32MP15x */
	U_BOOT_SUBCMD_MKENT(close, 2, 0, do_trustfence_close)
);
