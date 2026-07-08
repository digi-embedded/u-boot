// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 NXP
 */

#include "vsprintf.h"
#include <memalign.h>
#include <command.h>
#include <ctype.h>
#include <errno.h>
#include <imx_container.h>
#include <asm/io.h>
#include <asm/mach-imx/ele_api.h>
#include <asm/mach-imx/sys_proto.h>
#include <asm/arch-imx/cpu.h>
#include <asm/arch/sys_proto.h>
#include <console.h>
#include <cpu_func.h>
#include <asm/global_data.h>
#include <stdio.h>

static uint decode_digit(int ch)
{
	ch = tolower(ch);
	bool is_digit = ('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f');

  if (!is_digit)
		return 256;

	return ch <= '9' ? ch - '0' : ch - 'a' + 0xa;
}

static u8 hexstring_to_byte(const char *data)
{
	return decode_digit(data[0]) << 4 | decode_digit(data[1]);
}

static u32 hexstring_to_word(const char *data)
{
	return hexstring_to_byte(data)
	| hexstring_to_byte(data + 2) << 8
	| hexstring_to_byte(data + 4) << 16
	| hexstring_to_byte(data + 6) << 24;
}

static int decode_ele_message(struct ele_msg *msg, const char *data)
{
	u32 arglen = strlen(data);

	if (arglen < 8) {
		printf("Message lacks ELE header\n");
		return 1;
	}

	/* ELE header */
	msg->version = hexstring_to_byte(data + 0);
	msg->size    = hexstring_to_byte(data + 2);
	msg->command = hexstring_to_byte(data + 4);
	msg->tag     = hexstring_to_byte(data + 6);

	/* Number of words expected has to match number of received hex characters */
	if (arglen != msg->size * 8) {
		printf("Argument size %d bytes does not match the expected size %d bytes \n",
		       arglen, msg->size * 8);
		return 1;
	}

	/* Parse the remaining size - 1 words */
	for (u32 i = 0; i < msg->size - 1; i++)
		msg->data[i] = hexstring_to_word(data + (i + 1) * 8);

	return 0;
}

static void print_as_hexstring(char *msg, u32 size)
{
	for (u32 i = 0; i < size; i++)
		printf("%02x", msg[i]);
}

static void print_ele_message(struct ele_msg *msg)
{
	print_as_hexstring((char *) msg, msg->size * sizeof(u32));
}

static int do_ele_message(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct ele_msg msg = { 0 };
	int ret;

	if (argc < 4)
		return CMD_RET_USAGE;

	ulong ele_buffer_address = hextoul(argv[1], NULL);
	ulong ele_buffer_size    = hextoul(argv[2], NULL);

	/* We parse the ELE message encoded as hex string */
	ret = decode_ele_message(&msg, argv[3]);
	if (ret) {
		printf("Decode ELE message error\n");
		return CMD_RET_FAILURE;
	}

	/* Align cache maintenance range */
	ulong cache_start = ALIGN_DOWN(ele_buffer_address, ARCH_DMA_MINALIGN);
	ulong cache_end   = ALIGN(ele_buffer_address + ele_buffer_size, ARCH_DMA_MINALIGN);

	flush_dcache_range(cache_start, cache_end);

	/* We send the message to ELE and receive back the response */
	ret = ele_message_call(&msg);
	if (ret) {
		printf("Call ELE message error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	invalidate_dcache_range(cache_start, cache_end);

	/* ELE reponse is a message too */
	print_ele_message(&msg);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(ele_message, CONFIG_SYS_MAXARGS, 4, do_ele_message,
		"Send RAW message to ELE",
		"<addr> <size> <msg>\n"
		"addr - ELE Buffer start address\n"
		"size - ELE Buffer size in bytes\n"
		"msg  - RAW message encoded as hexstring\n"
);
