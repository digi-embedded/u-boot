// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024-2026 Digi International Inc.
 */

#include <config.h>
#include <firmware/imx/sci/sci.h>
#include <fuse.h>

#include "../common/trustfence.h"

int confirm_close(void);

bool trustfence_is_closed(void)
{
	sc_err_t err;
	uint16_t lc;

	err = sc_seco_chip_info(-1, &lc, NULL, NULL, NULL);
	if (err != SC_ERR_NONE) {
		printf("Error in get lifecycle\n");
		return true;
	}

	return ((lc & 0x80) == 0x80);
}

int close_device(int confirmed_close)
{
	int err;
	uint16_t lc;
	uint8_t idx = 0U;

	if (!confirmed_close && !confirm_close())
		return -EACCES;

	err = sc_seco_chip_info(-1, &lc, NULL, NULL, NULL);
	if (err != SC_ERR_NONE) {
		printf("Error in get lifecycle\n");
		return -EIO;
	}

	if (lc != 0x20) {
		printf("Current lifecycle is NOT NXP closed, can't move to OEM closed (0x%x)\n", lc);
		return -EPERM;
	}

	/* Verifiy that we do not have any AHAB events */
	err = sc_seco_get_event(-1, idx, NULL);
	if (err == SC_ERR_NONE) {
		printf ("SECO Event[%u] - abort closing device.\n", idx);
		return -EIO;
	}

	err = sc_seco_forward_lifecycle(-1, 16);
	if (err != SC_ERR_NONE) {
		printf("Error in forward lifecycle to OEM closed\n");
		return -EIO;
	}

	puts("Closing device...\n");

	return 0;
}

int revoke_keys(void)
{
	int err;
	u32 info = 0x10;

	err = sc_seco_commit(-1, &info);
	if (err != SC_ERR_NONE) {
		printf("%s: Error in sc_seco_commit\n", __func__);
		return -EIO;
	}

	return 0;
}

/* Trustfence helper functions */
int trustfence_status(void)
{
	uint8_t idx = 0U;
	int err;

	if (!is_usb_boot())
		printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted() ?
				"[YES]" : "[NO]");
	puts("* AHAB events:\t\t");
	err = sc_seco_get_event(-1, idx, NULL);
	if (err == SC_ERR_NONE)
		idx++;
	if (idx == 0)
		puts("[NO ERRORS]\n");
	else
		puts("[ERRORS PRESENT!]\n");

	return err;
}

void board_print_trustfence_jtag_mode(u32 *sjc)
{
	return;
}

void board_print_trustfence_jtag_key(u32 *sjc)
{
	return;
}

int sense_key_status(u32 *val)
{
	if (fuse_sense(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			val))
		return -1;

	*val = (*val >> CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET) &
		CONFIG_TRUSTFENCE_SRK_REVOKE_MASK;

	return 0;
}
