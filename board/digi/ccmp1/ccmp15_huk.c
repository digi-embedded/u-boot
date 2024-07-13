// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Digi International, Inc.
 */

#include <common.h>
#include <fuse.h>
#include <dm.h>

void board_parse_dt_huk(int *huk_otp_id, int *huk_words)
{
	int node = 0;
	int bsec_node;

	/* BSEC node */
	bsec_node = fdt_node_offset_by_compatible(gd->fdt_blob, -1,
						  "st,stm32mp15-bsec");
	fdt_for_each_subnode(node, gd->fdt_blob, bsec_node) {
		const fdt32_t *cuint = NULL;
		const char *string = NULL;
		int len = 0;

		string = fdt_get_name(gd->fdt_blob, node, &len);
		if (!string || !len)
			continue;

		if (!strncmp(string, "huk_otp@", strlen("huk_otp@"))) {
			cuint = fdt_getprop(gd->fdt_blob, node, "reg", &len);
			if (!cuint || (len != (2 * (int)sizeof(uint32_t))))
				continue;

			*huk_otp_id = fdt32_to_cpu(*cuint) / sizeof(uint32_t);
			*huk_words = fdt32_to_cpu(*(cuint + 1)) / 4;
			break;
		}
	}
}

int board_prog_huk(int huk_otp_id, int huk_words, uint32_t *huk)
{
	u32 bank = 0;
	int ret, i;

	for (i = 0; i < huk_words; i++) {
		ret = fuse_prog(bank, huk_otp_id + i, huk[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_lock_huk(int huk_otp_id, int huk_words)
{
	u32 bank = 0;
	int ret, i;

	for (i = 0; i < huk_words; i++) {
		ret = fuse_lock(bank, huk_otp_id + i);
		if (ret)
			return ret;
	}

	return 0;
}
