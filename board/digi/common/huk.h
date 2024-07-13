// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Digi International, Inc.
 */

#ifndef __HUK_H_
#define __HUK_H_

#define MAX_HUK_OTP_WORDS   4

void board_parse_dt_huk(int *huk_otp_id, int *huk_words);
int board_prog_huk(int huk_otp_id, int huk_words, uint32_t *huk);
int board_lock_huk(int huk_otp_id, int huk_words);

#endif	/* __HUK_H_ */
