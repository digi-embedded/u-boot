// SPDX-License-Identifier: GPL-2.0+
/**
 * Copyright 2024-2026 NXP
 */
#include <command.h>
#include <spl.h>
#include <stdlib.h>

#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/sys_proto.h>
#include <asm/mach-imx/qb.h>

static int get_board_boot_device(enum boot_device dev)
{
	switch (dev) {
	case SD1_BOOT:
	case MMC1_BOOT:
		return BOOT_DEVICE_MMC1;
	case SD2_BOOT:
	case MMC2_BOOT:
		return BOOT_DEVICE_MMC2;
	case USB_BOOT:
		return BOOT_DEVICE_BOARD;
	case QSPI_BOOT:
		return BOOT_DEVICE_SPI;
	default:
		return BOOT_DEVICE_NONE;
	}
}

static void parse_qb_args(int argc, char * const argv[],
			  int *qb_dev, int qb_bootdev)
{
	long dev = -1;
	char *interface = "";

	if (argc >= 2) {
		interface = argv[1];
	} else {
		/* qb save -> use boot device */
		*qb_dev = qb_bootdev;
	}

	if (argc == 3)
		dev = simple_strtol(argv[2], NULL, 10);

	if (!strcmp(interface, "mmc") && dev >= 0 &&
	    dev <= (BOOT_DEVICE_MMC2_2 - BOOT_DEVICE_MMC1))
		*qb_dev = BOOT_DEVICE_MMC1 + dev;

	if (!strcmp(interface, "spi"))
		*qb_dev = BOOT_DEVICE_SPI;
}

static int do_qb(struct cmd_tbl *cmdtp, int flag, int argc,
		 char * const argv[], bool save)
{
	int ret = CMD_RET_FAILURE;
	enum boot_device boot_dev = UNKNOWN_BOOT;
	int qb_dev = BOOT_DEVICE_NONE, qb_bootdev;

	boot_dev = get_boot_device();
	qb_bootdev = get_board_boot_device(boot_dev);

	parse_qb_args(argc, argv, &qb_dev, qb_bootdev);

	ret = qb(qb_dev, qb_bootdev, save);

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int do_qb_check(struct cmd_tbl *cmdtp, int flag,
		       int argc, char * const argv[])
{
	return qb_check() ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}

static int do_qb_save(struct cmd_tbl *cmdtp, int flag,
		      int argc, char * const argv[])
{
	return do_qb(cmdtp, flag, argc, argv, true);
}

static int do_qb_erase(struct cmd_tbl *cmdtp, int flag,
		       int argc, char * const argv[])
{
	return do_qb(cmdtp, flag, argc, argv, false);
}

static struct cmd_tbl cmd_qb[] = {
	U_BOOT_CMD_MKENT(check, 1, 1, do_qb_check, "", ""),
	U_BOOT_CMD_MKENT(save,  3, 1, do_qb_save,  "", ""),
	U_BOOT_CMD_MKENT(erase, 3, 1, do_qb_erase, "", ""),
};

static int do_qbops(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	struct cmd_tbl *cp;

	cp = find_cmd_tbl(argv[1], cmd_qb, ARRAY_SIZE(cmd_qb));

	/* Drop the qb command */
	argc--;
	argv++;

	if (!cp) {
		printf("%s cp is null\n", __func__);
		return CMD_RET_USAGE;
	}

	if (argc > cp->maxargs) {
		printf("%s argc(%d) > cp->maxargs(%d)\n", __func__, argc, cp->maxargs);
		return CMD_RET_USAGE;
	}

	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp)) {
		printf("%s %s repeat flag set  but command is not repeatable\n",
		       __func__, cp->name);
		return CMD_RET_SUCCESS;
	}

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	qb, 4, 1, do_qbops,
	"DDR Quick Boot sub system",
	"check - check if quick boot data is stored in mem by training flow\n"
	"qb save [interface] [dev]  - save quick boot data in NVM location    => trigger quick boot flow\n"
	"qb erase [interface] [dev] - erase quick boot data from NVM location => trigger training flow\n"
);
