/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2026 NXP */

#ifndef _EFI_ERASE_BLOCK_H
#define _EFI_ERASE_BLOCK_H

#include <efi_api.h>

#define EFI_ERASE_BLOCK_PROTOCOL_REVISION ((2 << 16) | 60)

struct efi_erase_block_token {
	struct efi_event *event;
	efi_status_t transaction_status;
};

struct efi_erase_block_protocol {
	u64 revision;
	u32 erase_length_granularity;
	efi_status_t (EFIAPI *erase_blocks)(struct efi_erase_block_protocol *this,
					    u32 media_id, u64 lba,
					    struct efi_erase_block_token *token,
					    efi_uintn_t size);
};

#endif
