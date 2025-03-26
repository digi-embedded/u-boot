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

struct stm32key {
	char *name;
	char *desc;
	u16 start;
	u8 size;
	int (*post_process)(struct udevice *dev, const struct stm32key *key);
};

struct otp_close {
	u32 word;
	u32 mask_wr;
	u32 mask_rd;
	bool (*close_status_ops)(u32 value, u32 mask);
};

#define STM32MP1_OTP_CLOSE_ID				0	/* OTP0 - CFG0 */
#define STM32_OTP_STM32MP13X_OPEN_MASK			0x17	/* BSEC_OTP_DATA0 : OTP_SECURED open device */
#define STM32_OTP_STM32MP13X_CLOSE_MASK 		0x3F	/* BSEC_OTP_DATA0 : OTP_SECURED closed device */
#define STM32_OTP_STM32MP13X_BSCANDIS_MASK		0x17F	/* BSEC_OTP_DATA0 : OTP_SECURED closed device with boundary scan disabled */
#define STM32_OTP_STM32MP13X_JTAGDIS_MASK		0x3FF	/* BSEC_OTP_DATA0 : OTP_SECURED closed device with JTAG disabled */
#define STM32_OTP_STM32MP15X_CLOSE_MASK 		BIT(6)	/* BSEC_OTP_DATA0 : OTP_SECURED closed device */
#define STM32MP2X_OTP_CLOSE_ID				18	/* OTP18 - BOOTROM_CONFIG_9 */
#define STM32_OTP_STM32MP2X_CLOSE_MASK			0xF	/* BOOTROM_CONFIG_9 : 0-3 SECURE_BOOT */
#define STM32_OTP_STM32MP2X_PROVISIONING_DONE_MASK	0XF0	/* BOOTROM_CONFIG_9 : 4-7 PROV_DONE */

enum jtag_status {
	JTAG_OPEN,
	BSCAN_DISABLED,
	JTAG_DISABLED,
	JTAG_UNKNOWN,
};
char *jtag_desc[] = {
	"[OPEN]",
	"[BSCAN disabled]",
	"[DISABLED]",
	"[UNKNOWN]",
};

/* Functions defined in cmd_stm32key.c */
extern int get_misc_dev(struct udevice **dev);
extern const struct stm32key *get_key(u8 index);
extern u8 get_key_nb(void);
extern int read_key_otp(struct udevice *dev, const struct stm32key *key, bool print, bool *locked);
extern void read_key_value(const struct stm32key *key, u32 addr);
extern int fuse_key_value(struct udevice *dev, const struct stm32key *key, u32 addr, bool print);
extern int do_stm32key_close(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]);
extern u8 get_otp_close_state_nb(void);
extern const struct otp_close *get_otp_close_state(u8 index);
extern bool stm32mp_is_closed(void);

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
	int index = 0; /* PKH / PKHTH / OEM-KEY1 */

	if ((IS_ENABLED(CONFIG_STM32MP25X) || IS_ENABLED(CONFIG_STM32MP23X)) &&
	    strcmp(cmdtp->name, "read_pkhth_m") == 0) {
		index = 1; /* OEM-KEY2 */
	}

	return trustfence_read_key(argc, argv, index);
}

static int do_trustfence_prog_pkh(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int index = 0; /* PKH / PKHTH / OEM-KEY1 */

	if ((IS_ENABLED(CONFIG_STM32MP25X) || IS_ENABLED(CONFIG_STM32MP23X)) &&
	    strcmp(cmdtp->name, "prog_pkhth_m") == 0) {
		index = 1; /* OEM-KEY2 */
	}

	return trustfence_prog_key(argc, argv, index);
}

static int read_otp_jtag_mode(struct udevice *dev, int *jtag)
{
	int ret, i;
	u32 val = 0, mask = 0, word = 0;
	u32 otp_close_nb = get_otp_close_state_nb();
	const struct otp_close *otp_close = NULL;
	bool closed = false;

	/* Obtain data for close word */
	for (i = 0; i < otp_close_nb; i++) {
		otp_close = get_otp_close_state(i);
		if (otp_close && otp_close->close_status_ops)
			break;
	}

	if (i >= otp_close_nb) {
		printf("Error: no OTP close info found\n");
		return -1;
	}

	mask = otp_close->mask_rd;
	word = otp_close->word;

	/* Read OTP CLOSE ID */
	ret = misc_read(dev, STM32_BSEC_OTP(word), &val, 4);
	if (ret < 0) {
		printf("Error: can't read OTP mode\n");
		return -1;
	}

	closed = otp_close->close_status_ops(val, mask);

	if (IS_ENABLED(CONFIG_STM32MP15X)) {
		*jtag = closed ? JTAG_DISABLED : JTAG_OPEN;
	} else if (IS_ENABLED(CONFIG_STM32MP13X)) {
		switch (val) {
		case STM32_OTP_STM32MP13X_OPEN_MASK:
		case STM32_OTP_STM32MP13X_CLOSE_MASK:
			*jtag = JTAG_OPEN;
			break;
		case STM32_OTP_STM32MP13X_BSCANDIS_MASK:
			*jtag = BSCAN_DISABLED;
			break;
		case STM32_OTP_STM32MP13X_JTAGDIS_MASK:
			*jtag = JTAG_DISABLED;
			break;
		default:
			printf("%s: Error: invalid OTP mode value: 0x%08x\n", __func__, val);
			return -1;
		}
	} else {
		*jtag = JTAG_UNKNOWN;
	}

	return 0;
}

#ifndef CONFIG_STM32MP15X
static int do_trustfence_read_edmk(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int index = 1; /* EDMK */

	if (IS_ENABLED(CONFIG_STM32MP25X) || IS_ENABLED(CONFIG_STM32MP23X)) {
		if (strcmp(cmdtp->name, "read_edmk_fip") == 0) {
			index = 2; /* FIP-EDMK */
		} else if (strcmp(cmdtp->name, "read_edmk") == 0) {
			index = 3; /* EDMK1 */
		} else if (strcmp(cmdtp->name, "read_edmk_m") == 0) {
			index = 4; /* EDMK2 */
		}
	}

	return trustfence_read_key(argc, argv, index);
}

static int do_trustfence_prog_edmk(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	// TODO: enable when encryption of bootloader fully supported */
	printf("Encryption of bootloader not yet supported. Aborting.\n");
	return 0;
	// int index = 1; /* EDMK */

	// if (IS_ENABLED(CONFIG_STM32MP25X) || IS_ENABLED(CONFIG_STM32MP23X)) {
	//	if (strcmp(cmdtp->name, "prog_edmk_fip") == 0) {
	//		index = 2; /* FIP-EDMK */
	//	} else if (strcmp(cmdtp->name, "prog_edmk") == 0) {
	//		index = 3; /* EDMK1 */
	//	} else if (strcmp(cmdtp->name, "prog_edmk_m") == 0) {
	//		index = 4; /* EDMK2 */
	//	}
	//}
	//return trustfence_prog_key(argc, argv, index);
}
#endif /* !CONFIG_STM32MP15X */

#ifdef CONFIG_STM32MP13X
static int do_trustfence_prog_jtag(struct cmd_tbl *cmdtp, int flag, int argc,
				   char *const argv[])
{
	struct udevice *dev;
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
		val = STM32_OTP_STM32MP13X_BSCANDIS_MASK;
	} else if (!strcmp(argv[argc - 1], "disable-jtag")) {
		newmode = JTAG_DISABLED;
		val = STM32_OTP_STM32MP13X_JTAGDIS_MASK;
	} else {
		return CMD_RET_USAGE;
	}

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	/* Read OTP JTAG status */
	ret = read_otp_jtag_mode(dev, &jtag);
	if (ret < 0)
		return CMD_RET_FAILURE;

	/* Abort if the device is not closed or mode already programmed */
	if (!stm32mp_is_closed()) {
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

	ret = misc_write(dev, STM32_BSEC_OTP(STM32MP1_OTP_CLOSE_ID), &val, 4);
	if (ret != 4) {
		printf("Error: can't update OTP %d\n", STM32MP1_OTP_CLOSE_ID);
		return CMD_RET_FAILURE;
	}
	printf("Secure JTAG programmed!\n");

	return CMD_RET_SUCCESS;
}
#endif /* CONFIG_STM32MP13X */

static int do_trustfence_status(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct udevice *dev;
	int ret;
	int jtag;

	ret = get_misc_dev(&dev);
	if (ret)
		return CMD_RET_FAILURE;

	for (int i = 0; i < get_key_nb(); i++)
		read_key_otp_stat(dev, i);

	printf("* Secure boot:\t%s\n", stm32mp_is_closed() ? "[CLOSED]" : "[OPEN]");
	if (IS_ENABLED(CONFIG_STM32MP13X) || IS_ENABLED(CONFIG_STM32MP15X)) {
		/* Read OTP JTAG status */
		ret = read_otp_jtag_mode(dev, &jtag);
		if (ret < 0)
			return CMD_RET_FAILURE;
		printf("* JTAG:       \t%s\n", jtag_desc[jtag]);
	}

	return CMD_RET_SUCCESS;
}

static int do_trustfence_close(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return do_stm32key_close(cmdtp, flag, argc, argv);
}

U_BOOT_CMD_WITH_SUBCMDS(trustfence, "Digi TrustFence(TM) command",
	"status - show secure boot configuration status\n"
#ifdef CONFIG_STM32MP15X
	"trustfence read_pkh [<addr>] - read Public Key Hash (PKH) at <addr> or all key in OTP\n"
	"trustfence prog_pkh [-y] <addr> <size in bytes> - burn Public Key Hash (PKH) (PERMANENT)\n"
#else
	"trustfence read_pkhth [<addr>] - read Public Key Hash Table Hash (PKHTH) at <addr> or all key in OTP\n"
	"trustfence prog_pkhth [-y] <addr> - burn Public Key Hash Table Hash (PKHTH) (PERMANENT)\n"
#ifdef CONFIG_STM32MP25X
	"trustfence read_pkhth_m [<addr>] - read Public Key Hash Table Hash (PKHTH)for FSBLM at <addr> or all key in OTP\n"
	"trustfence prog_pkhth_m [-y] <addr> - burn Public Key Hash Table Hash (PKHTH) for FSBLM (PERMANENT)\n"
#endif /* CONFIG_STM32MP25X */
	"trustfence read_edmk [<addr>] - read Encryption Decryption master key (EDMK) at <addr> or all key in OTP\n"
	"trustfence prog_edmk [-y] <addr> - burn Encryption Decryption master key (EDMK) (PERMANENT)\n"
#ifdef CONFIG_STM32MP25X
	"trustfence read_edmk_m [<addr>] - read Encryption Decryption master key (EDMK) for FSBLM at <addr> or all key in OTP\n"
	"trustfence prog_edmk_m [-y] <addr> - burn Encryption Decryption master key (EDMK) for FSBLM (PERMANENT)\n"
	"trustfence read_edmk_fip [<addr>] - read Encryption Decryption master key (EDMK) for FIP at <addr> or all key in OTP\n"
	"trustfence prog_edmk_fip [-y] <addr> - burn Encryption Decryption master key (EDMK) for FIP (PERMANENT)\n"
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
	U_BOOT_SUBCMD_MKENT(read_pkh, 2, 0, do_trustfence_read_pkh),
	U_BOOT_SUBCMD_MKENT(prog_pkh, 3, 0, do_trustfence_prog_pkh),
#else
	U_BOOT_SUBCMD_MKENT(read_pkhth, 2, 0, do_trustfence_read_pkh),
	U_BOOT_SUBCMD_MKENT(prog_pkhth, 3, 0, do_trustfence_prog_pkh),
#ifdef CONFIG_STM32MP25X
	U_BOOT_SUBCMD_MKENT(read_pkhth_m, 2, 0, do_trustfence_read_pkh),
	U_BOOT_SUBCMD_MKENT(prog_pkhth_m, 3, 0, do_trustfence_prog_pkh),
#endif /* CONFIG_STM32MP25X */
	U_BOOT_SUBCMD_MKENT(read_edmk, 2, 0, do_trustfence_read_edmk),
	U_BOOT_SUBCMD_MKENT(prog_edmk, 3, 0, do_trustfence_prog_edmk),
#ifdef CONFIG_STM32MP25X
	U_BOOT_SUBCMD_MKENT(read_edmk_m, 2, 0, do_trustfence_read_edmk),
	U_BOOT_SUBCMD_MKENT(prog_edmk_m, 3, 0, do_trustfence_prog_edmk),
	U_BOOT_SUBCMD_MKENT(read_edmk_fip, 2, 0, do_trustfence_read_edmk),
	U_BOOT_SUBCMD_MKENT(prog_edmk_fip, 3, 0, do_trustfence_prog_edmk),
#endif /* CONFIG_STM32MP25X */
#ifdef CONFIG_STM32MP13X
	U_BOOT_SUBCMD_MKENT(prog_jtag, 2, 0, do_trustfence_prog_jtag),
#endif /* CONFIG_STM32MP13X */
#endif /* CONFIG_STM32MP15X */
	U_BOOT_SUBCMD_MKENT(close, 2, 0, do_trustfence_close)
);
