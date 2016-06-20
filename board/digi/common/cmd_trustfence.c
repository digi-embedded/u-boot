/*
 * (C) Copyright 2016 Digi International, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <fuse.h>
#include <asm/arch/hab.h>
#include <asm/errno.h>
#include "helper.h"

int fuse_check_srk(void);
int fuse_prog_srk(u32 addr, u32 size);

__weak int close_device(void)
{
	return fuse_prog(CONFIG_TRUSTFENCE_CLOSE_BIT_BANK,
			 CONFIG_TRUSTFENCE_CLOSE_BIT_WORD,
			 1 << CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET);
}

__weak int revoke_key_index(int i)
{
	u32 val = (1 << i) << CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET;
	return fuse_prog(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			 CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			 val);
}

__weak int sense_key_status(u32 *val)
{
	if (fuse_sense(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			val))
		return -1;

	*val = (*val & CONFIG_TRUSTFENCE_SRK_REVOKE_MASK) >>
		CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET;

	return 0;
}

static void board_print_trustfence_jtag_mode(u32 *sjc)
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

static void board_print_trustfence_jtag_key(u32 *sjc)
{
	int i;

	for (i = CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.8x", sjc[i]);
	printf("\n");

	/* Formatted printout */
	printf("    Secure JTAG response Key: 0x%x%x\n", sjc[1], sjc[0]);
}

static int do_trustfence(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 bank = CONFIG_TRUSTFENCE_JTAG_MODE_BANK;
	u32 word = CONFIG_TRUSTFENCE_JTAG_MODE_START_WORD;
	u32 val[2], addr;
	char *jtag_op = NULL;
	int ret = -1, i = 0;
	hab_rvt_report_status_t *hab_report_status = hab_rvt_report_status_p;

	if (argc < 2)
		return CMD_RET_USAGE;

	op = argv[1];
	argc -= 2 + confirmed;
	argv += 2 + confirmed;

	if (!strcmp(op, "prog_srk")) {
		if (argc < 2)
			return CMD_RET_USAGE;

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		ret = fuse_check_srk();
		if (ret == 0) {
			puts("SRK efuses are already burned!\n");
			return CMD_RET_FAILURE;
		} else if (ret < 0) {
			goto err;
		}

		if (strtou32(argv[0], 16, &addr))
			return CMD_RET_USAGE;

		if (strtou32(argv[1], 16, &val[0]))
			return CMD_RET_USAGE;

		puts("Programming SRK efuses... ");
		if (fuse_prog_srk(addr, val[0]))
			goto err;
		puts("[OK]\n");
	} else if (!strcmp(op, "close")) {
		enum hab_config config = 0;
		enum hab_state state = 0;

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		puts("Checking SRK bank...\n");
		ret = fuse_check_srk();
		if (ret > 0) {
			puts("[ERROR] Burn the SRK OTP bits before "
			     "closing the device.\n");
			return CMD_RET_FAILURE;
		} else if (ret < 0) {
			goto err;
		}

		if (hab_report_status(&config, &state) != HAB_SUCCESS) {
			puts("[ERROR]\n There are HAB Events which will "
			     "prevent the target from booting once closed.\n");
			puts("Run 'hab_status' and check the errors.\n");
			return CMD_RET_FAILURE;
		}

		puts("Closing device...\n");
		if (close_device())
			goto err;
		puts("[OK]\n");
	} else if (!strcmp(op, "revoke")) {
		if (argc < 1)
			return CMD_RET_USAGE;

		if (strtou32(argv[0], 16, &val[0]))
			return CMD_RET_FAILURE;

		if (val[0] == CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS) {
			puts("Last key cannot be revoked.\n");
			return CMD_RET_FAILURE;
		} else if (val[0] > CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS) {
			puts("Invalid key index.\n");
			return CMD_RET_FAILURE;
		}

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		printf("Revoking key index %d...", val[0]);
		if (revoke_key_index(val[0]))
			goto err;
		puts("[OK]\n");
	} else if (!strcmp(op, "status")) {
		int key_index;
		enum hab_config config = 0;
		enum hab_state state = 0;

		printf("* SRK fuses:\t");
		ret = fuse_check_srk();
		if (ret > 0) {
			printf("[NOT PROGRAMMED]\n");
		} else if (ret == 0) {
			puts("[PROGRAMMED]\n");
		} else {
			puts("[ERROR]\n");
		}

		if (sense_key_status(&val[0]))
			goto err;
		for (key_index = 0; key_index < CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS;
		     key_index++) {
			printf("   Key %d:\t", key_index);
			printf((val[0] & (1 << key_index) ?
	                       "[REVOKED]\n" : "[OK]\n"));
		}
		printf("   Key %d:\t[OK]\n", CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS);

		printf("* Secure boot:\t%s", is_hab_enabled() ?
		       "[CLOSED]\n" : "[OPEN]\n");

		puts("* HAB events:\t");
		if (hab_report_status(&config, &state) == HAB_SUCCESS)
			puts("[NO ERRORS]\n");
		else
			puts("[ERRORS PRESENT!]\n");
	} else if (!strcmp(op, "update")) {
		char cmd_buf[CONFIG_SYS_CBSIZE];
		unsigned long loadaddr = getenv_ulong("loadaddr", 16,
						      CONFIG_LOADADDR);
		unsigned long filesize;
		unsigned long dek_blob_src;
		unsigned long dek_blob_dst;
		unsigned long dek_blob_final_dst;

		if (argc == 3 && (!strcmp(argv[0], "tftp") ||
				  !strcmp(argv[0], "nfs"))) {
			sprintf(cmd_buf, "%s $loadaddr %s", argv[0],
				argv[1]);
		} else if (argc == 4 && !strcmp(argv[0], "mmc")) {
			sprintf(cmd_buf, "load mmc %s $loadaddr %s",
				argv[1], argv[2]);
		} else
			return CMD_RET_USAGE;

		printf("\nLoading encrypted U-Boot image...\n");
		if (run_command(cmd_buf, 0))
			return CMD_RET_FAILURE;

		filesize = getenv_ulong("filesize", 16, 0);
		/* DEK blob will be directly appended to the U-Boot image */
		dek_blob_final_dst = loadaddr + filesize;
		/*
		 * for the DEK blob source (DEK in plain text) we use the
		 * first 0x100 aligned memory address
		 */
		dek_blob_src = (dek_blob_final_dst & 0xFFFFFF00) + 0x100;
		/*
		 * DEK destination also needs to be 0x100 aligned. Leave
		 * 0x400 = 1KiB to fit the DEK source
		 */
		dek_blob_dst = dek_blob_src + 0x400;

		printf("\nLoading Data Encryption Key...\n");
		if (argc == 3) {
			sprintf(cmd_buf, "%s 0x%lx %s", argv[0], dek_blob_src,
				argv[2]);
		} else { /* argc == 4 */
			sprintf(cmd_buf, "load mmc %s 0x%lx %s", argv[1],
				dek_blob_src, argv[3]);
		}
		if (run_command(cmd_buf, 0))
			return CMD_RET_FAILURE;
		filesize = getenv_ulong("filesize", 16, 0);

		printf("\nGenerating DEK blob...\n");
		/* dek_blob takes size in bits */
		sprintf(cmd_buf, "dek_blob 0x%lx 0x%lx 0x%lx",
			dek_blob_src, dek_blob_dst, filesize * 8);
		if (run_command(cmd_buf, 0))
			return CMD_RET_FAILURE;

		/*
		 * Set filesize to the size of the DEK blob, that is:
		 * header (8 bytes) + random AES-256 key (32 bytes)
                 * + DEK ('filesize' bytes) + MAC (16 bytes)
		 */
		filesize += 8 + 32 + 16;

		/* Copy DEK blob to its final destination */
		memcpy((void *)dek_blob_final_dst, (void *)dek_blob_dst,
			filesize);

		printf("\nFlashing U-Boot partition...\n");
		sprintf(cmd_buf, "update uboot ram 0x%lx 0x%lx",
			loadaddr,
			dek_blob_final_dst + filesize - loadaddr);
		setenv("forced_update", "y");
		ret = run_command(cmd_buf, 0);
		setenv("forced_update", "n");
		if (ret)
			return CMD_RET_FAILURE;
	} else if (!strcmp(op, "jtag")) {
		if (!strcmp(argv[0], "read")) {
			printf("Reading Secure JTAG mode: ");
			ret = fuse_read(bank, word, &val[0]);
			if (ret)
				goto err;
			board_print_trustfence_jtag_mode(val);
		} else if (!strcmp(argv[0], "read_key")) {
			bank = CONFIG_TRUSTFENCE_JTAG_KEY_BANK;
			word = CONFIG_TRUSTFENCE_JTAG_KEY_START_WORD;
			printf("Reading response key: ");
			for (i = 0; i < CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER; i++, word++) {
				ret = fuse_read(bank, word, &val[i]);
				if (ret)
					goto err;
			}
			board_print_trustfence_jtag_key(val);
		} else if (!strcmp(argv[0], "prog")) {
			if (!confirmed && !confirm_prog())
				return CMD_RET_FAILURE;

			if (argc < 1)
				return CMD_RET_USAGE;

			strcpy(jtag_op, argv[1]);

			printf("Programming Secure JTAG mode... ");
			if (!strcmp(jtag_op, "secure")) {
				ret = fuse_prog(bank, word, TRUSTFENCE_JTAG_ENABLE_SECURE_JTAG_MODE);
			} else if (!strcmp(jtag_op, "disable-debug")) {
				ret = fuse_prog(bank, word, TRUSTFENCE_JTAG_DISABLE_DEBUG);
			} else if (!strcmp(jtag_op, "disable-jtag")) {
				ret = fuse_prog(bank, word, TRUSTFENCE_JTAG_DISABLE_JTAG);
			} else {
				printf("\nWrong parameter.\n");
				ret = CMD_RET_USAGE;
				goto err;
			}
			if (ret)
				goto err;
			printf("[OK]\n");
		} else if (!strcmp(argv[0], "prog_key")) {
			if (argc < CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER)
				return CMD_RET_USAGE;

			if (!confirmed && !confirm_prog())
				return CMD_RET_FAILURE;
			printf("Programming response key... ");
			/* Write backwards, from MSB to LSB */
			word = CONFIG_TRUSTFENCE_JTAG_KEY_START_WORD +
				CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER - 1;
			bank = CONFIG_TRUSTFENCE_JTAG_KEY_BANK;
			for (i = 0; i < CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER; i++, word--) {
				if (strtou32(argv[i+1], 16, &val[i])) {
					ret = CMD_RET_USAGE;
					goto err;
				}

				ret = fuse_prog(bank, word, val[i]);
				if (ret)
					goto err;
			}
			printf("[OK]\n");
#if defined CONFIG_TRUSTFENCE_JTAG_OVERRIDE
		} else if (!strcmp(argv[0], "override")) {
			if (argc < 1)
				return CMD_RET_USAGE;

			strcpy(jtag_op, argv[1]);

			printf("Overriding Secure JTAG mode... ");
			if (!strcmp(jtag_op, "secure")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_ENABLE_SECURE_JTAG_MODE);
			} else if (!strcmp(jtag_op, "disable-debug")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_DISABLE_DEBUG);
			} else if (!strcmp(jtag_op, "disable-jtag")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_DISABLE_JTAG);
			} else {
				printf("\nWrong parameter.\n");
				ret = CMD_RET_USAGE;
				goto err;
			}
			if (ret)
				goto err;
			printf("[OK]\n");
		} else if (!strcmp(argv[0], "override_key")) {
			if (argc < CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER)
				return CMD_RET_USAGE;

			printf("Overriding response key... ");
			/* Write backwards, from MSB to LSB */
			word = CONFIG_TRUSTFENCE_JTAG_KEY_START_WORD + CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER - 1;
			bank = CONFIG_TRUSTFENCE_JTAG_KEY_BANK;
			for (i = 0; i < CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER; i++, word--) {
				if (strtou32(argv[i+1], 16, &val[i])) {
					ret = CMD_RET_USAGE;
					goto err;
				}

				ret = fuse_override(bank, word, val[i]);
				if (ret)
					goto err;
			}
			printf("[OK]\n");
#endif
		} else if (!strcmp(argv[0], "lock")) {
			if (!confirmed && !confirm_prog())
				return CMD_RET_FAILURE;
			printf("Locking Secure JTAG mode... ");
			ret = fuse_prog(OCOTP_LOCK_BANK,
					OCOTP_LOCK_WORD,
					CONFIG_TRUSTFENCE_JTAG_LOCK_FUSE);
			if (ret)
				goto err;
			printf("[OK]\n");
			printf("Locking of the Secure JTAG will be effective when the CPU is reset\n");
		} else if (!strcmp(argv[0], "lock_key")) {
			if (!confirmed && !confirm_prog())
				return CMD_RET_FAILURE;
			printf("Locking response key... ");
			ret = fuse_prog(OCOTP_LOCK_BANK,
					OCOTP_LOCK_WORD,
					CONFIG_TRUSTFENCE_JTAG_KEY_LOCK_FUSE);
			if (ret)
				goto err;
			printf("[OK]\n");
			printf("Locking of the response key will be effective when the CPU is reset\n");
		} else {
			return CMD_RET_USAGE;
		}
	} else {
		printf("[ERROR]\n");
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("[ERROR]\n");
	return ret;
}

U_BOOT_CMD(
	trustfence, CONFIG_SYS_MAXARGS, 0, do_trustfence,
	"Digi Trustfence(TM) command",
	"\r-- SECURE BOOT --\n"
	"trustfence status - show secure boot configuration status\n"
	"trustfence prog_srk [-y] <ram addr> <size in bytes> - burn SRK efuses (PERMANENT)\n"
	"trustfence close [-y] - close the device so that it can only boot "
			      "signed images (PERMANENT)\n"
	"trustfence revoke [-y] <key index> - revoke one Super Root Key (PERMANENT)\n"
	"trustfence update <source> [extra-args...]\n"
	" Description: flash an encrypted U-Boot image.\n"
	" Arguments:\n"
	"\n\t- <source>: tftp|nfs|mmc\n"
	"\n\tsource=tftp|nfs -> <uboot_file> <dek_file>\n"
	"\t\t - <uboot_file>: name of the encrypted uboot image\n"
	"\t\t - <dek_file>: name of the Data Encryption Key (DEK) in plain text\n"
	"\n\tsource=mmc -> <dev:part> <uboot_file> <dek_file>\n"
	"\t\t - <dev:part>: number of device and partition\n"
	"\t\t - <uboot_file>: name of the encrypted uboot image\n"
	"\t\t - <dek_file>: name of the Data Encryption Key (DEK) in plain text\n"
	"\nWARNING: These commands (except 'status' and 'update') burn the"
	" eFuses.\nThey are irreversible and could brick your device.\n"
	"Make sure you know what you do before playing with this command.\n"
	"\n-- SECURE JTAG --\n"
	"trustfence jtag read - read Secure JTAG mode from shadow registers\n"
	"trustfence jtag read_key - read Secure JTAG response key from shadow registers\n"
	"trustfence jtag prog [-y] <mode> - program Secure JTAG mode <mode> (PERMANENT). <mode> can be one of:\n"
	"    secure - Secure JTAG mode (debugging only possible by providing the key "
	"burned in the e-fuses)\n"
	"    disable-debug - JTAG debugging disabled (only boundary-scan possible)\n"
	"    disable-jtag - JTAG port disabled (no JTAG operations allowed)\n"
	"trustfence jtag prog_key [-y] <high_word> <low_word> - program response key (PERMANENT)\n"
#if defined CONFIG_TRUSTFENCE_JTAG_OVERRIDE
	"trustfence jtag override <mode> - override Secure JTAG mode <mode>. <mode> can be one of:\n"
	"    secure - Secure JTAG mode (debugging only possible by providing the key "
	"burned in the e-fuses)\n"
	"    disable-debug - JTAG debugging disabled (only boundary-scan possible)\n"
	"    disable-jtag - JTAG port disabled (no JTAG operations allowed)\n"
	"trustfence jtag override_key <high_word> <low_word> - override response key\n"
#endif
	"trustfence jtag lock [-y] - lock Secure JTAG mode and disable JTAG interface "
				"OTP bits (PERMANENT)\n"
	"trustfence jtag lock_key [-y] - lock Secure JTAG key OTP bits (PERMANENT)\n"
);
