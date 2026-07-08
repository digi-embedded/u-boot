/*
 * Copyright (C) 2024, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */


#include <config.h>

#include "jtag.h"

__weak void board_print_trustfence_jtag_mode(u32 *sjc)
{
	u32 sjc_mode;

	printf(" %.8x\n", sjc[0]);

	/* Formatted printout */
	if ((sjc_mode = (sjc[0] >> TRUSTFENCE_JTAG_DISABLE_OFFSET) &
			 TRUSTFENCE_JTAG_DISABLE_JTAG_MASK)){
		/* Read SJC_DISABLE */
		printf("    Secure JTAG disabled\n");
	} else {
		/* read JTAG_SMODE */
		sjc_mode = (sjc[0] >> TRUSTFENCE_JTAG_SMODE_OFFSET) &
			    TRUSTFENCE_JTAG_SMODE_MASK;
		if (sjc_mode == TRUSTFENCE_JTAG_SMODE_ENABLE)
			printf("    JTAG enable mode\n");
		else if (sjc_mode == TRUSTFENCE_JTAG_SMODE_SECURE)
			printf("    Secure JTAG mode\n");
		else if (sjc_mode == TRUSTFENCE_JTAG_SMODE_NO_DEBUG)
			printf("    No debug mode\n");
		else
			printf("    Unknow mode\n");
	}
}

__weak void board_print_trustfence_jtag_key(u32 *sjc)
{
	int i;

	for (i = CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", sjc[i]);
	printf("\n");

	/* Formatted printout */
	printf("    Secure JTAG response Key: 0x%x%x\n", sjc[1], sjc[0]);
}
