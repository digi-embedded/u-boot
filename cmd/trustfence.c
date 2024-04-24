/*
 * (C) Copyright 2016-2018 Digi International, Inc.
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
#include <fdt_support.h>
#include <fsl_sec.h>
#include <fuse.h>
#ifdef CONFIG_ARCH_IMX8
#include <asm/arch/sci/sci.h>
#endif
#include <asm/mach-imx/hab.h>
#include <linux/errno.h>
#include <u-boot/sha256.h>
#include <watchdog.h>
#include <malloc.h>
#include <mapmem.h>
#include <memalign.h>
#ifdef CONFIG_CONSOLE_ENABLE_GPIO
#include <asm/gpio.h>
#endif
#include <u-boot/md5.h>
#include <console.h>

#include "../board/digi/common/helper.h"
#include "../board/digi/common/trustfence.h"


#define UBOOT_HEADER_SIZE	0xC00
#define UBOOT_START_ADDR	(CONFIG_TEXT_BASE - UBOOT_HEADER_SIZE)

#define BLOB_DEK_OFFSET		0x100

/*
 * If CONFIG_CSF_SIZE is undefined, assume 0x4000. This value will be used
 * in the signing script.
 */
#ifndef CONFIG_CSF_SIZE
#define CONFIG_CSF_SIZE 0x4000
#endif

extern int rng_swtest_status;

#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))
#define DMA_ALIGN_UP(x) ALIGN_UP(x, ARCH_DMA_MINALIGN)

/*
 * Copy the DEK blob used by the current U-Boot image into a buffer. Also
 * get its size in the last out parameter.
 * Possible DEK key sizes are 128, 192 and 256 bits.
 * DEK blobs have an overhead of 56 bytes.
 * Hence, possible DEK blob sizes are 72, 80 and 88 bytes.
 *
 * The output buffer should be at least MAX_DEK_BLOB_SIZE (88) bytes long to
 * prevent out of boundary access.
 *
 * Returns 0 if the DEK blob was found, 1 otherwise.
 */
 /* TODO: also CONFIG_CC6 but still not migrated */
#if defined(CONFIG_CC6UL)
__weak int get_dek_blob(char *output, u32 *size)
{
	struct ivt *ivt = (struct ivt *)UBOOT_START_ADDR;

	/* Verify the pointer is pointing at an actual IVT table */
	if ((ivt->hdr.magic != IVT_HEADER_MAGIC) ||
	    (be16_to_cpu(ivt->hdr.length) != IVT_TOTAL_LENGTH))
		return 1;

	if (ivt->csf) {
		int blob_size = MAX_DEK_BLOB_SIZE;
		uint8_t *dek_blob = (uint8_t *)(uintptr_t)(ivt->csf +
				    CONFIG_CSF_SIZE - blob_size);

		/*
		 * Several DEK sizes can be used.
		 * Determine the size and the start of the DEK blob by looking
		 * for its header.
		 */
		while (*dek_blob != HDR_TAG && blob_size > 0) {
			dek_blob += 8;
			blob_size -= 8;
		}

		if (blob_size > 0) {
			*size = blob_size;
			memcpy(output, dek_blob, blob_size);
			return 0;
		}
	}

	return 1;
}
#else
__weak int get_dek_blob(ulong addr, u32 *size)
{
	return 1;
}
#endif

#ifdef CONFIG_AHAB_BOOT
extern int get_dek_blob_offset(ulong addr, u32 *offset);
extern int get_dek_blob_size(ulong addr, u32 *size);
extern int get_srk_revoke_mask(u32 *mask);
#endif
#ifdef CONFIG_ARCH_IMX8M
extern int get_dek_blob_offset(char *address, u32 *offset);
#endif

int is_uboot_encrypted() {
	char dek_blob[MAX_DEK_BLOB_SIZE];
	u32 dek_blob_size;

	/* U-Boot is encrypted if and only if get_dek_blob does not fail */
	return !get_dek_blob((ulong)dek_blob, &dek_blob_size);
}

int get_trustfence_key_modifier(unsigned char key_modifier[16])
{
	u32 ocotp_hwid[CONFIG_HWID_WORDS_NUMBER];
	int i, ret;

	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		ret = fuse_read(CONFIG_HWID_BANK,
				CONFIG_HWID_START_WORD + i,
				&ocotp_hwid[i]);
		if (ret)
			return ret;
	}
	md5((unsigned char *)(&ocotp_hwid), sizeof(ocotp_hwid), key_modifier);
	return ret;
}

/*
 * For secure OS, we want to have the DEK blob in a common absolute
 * memory address, so that there are no dependencies between the CSF
 * appended to the uImage and the U-Boot image size.
 * This copies the DEK blob into $loadaddr - BLOB_DEK_OFFSET. That is the
 * smallest negative offset that guarantees that the DEK blob fits and that it
 * is properly aligned.
 */
void copy_dek(void)
{
	ulong loadaddr = env_get_ulong("loadaddr", 16, CONFIG_SYS_LOAD_ADDR);
	ulong dek_blob_dst = loadaddr - BLOB_DEK_OFFSET;
	u32 dek_size;

	get_dek_blob(dek_blob_dst, &dek_size);
}

void copy_spl_dek(void)
{
	ulong loadaddr = env_get_ulong("loadaddr", 16, CONFIG_SYS_LOAD_ADDR);
	ulong dek_blob_dst = loadaddr - (2 * BLOB_DEK_OFFSET);

	get_dek_blob(dek_blob_dst, NULL);
}

/*
 * Check if all SRK words have been burned.
 *
 * Returns:
 * 0 if all SRK are burned
 * i+1 if SRK word i is not burned
 * <0 on error
 */
__weak int fuse_check_srk(void)
{
	int i;
	u32 val;
	int bank, word;

	for (i = 0; i < CONFIG_TRUSTFENCE_SRK_WORDS; i++) {
		bank = CONFIG_TRUSTFENCE_SRK_BANK +
		       (i / CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK);
		word = i % CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK;
		if (fuse_sense(bank, word+CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET,
			       &val))
			return -1;
		if (val == 0)
			return i + 1;
	}

	return 0;
}

__weak int fuse_prog_srk(u32 addr, u32 size)
{
	int i;
	int ret;
	int bank, word;
	uint32_t *src_addr = map_sysmem(addr, size);

	if (size != CONFIG_TRUSTFENCE_SRK_WORDS * 4) {
		puts("Bad size\n");
		return -1;
	}

	for (i = 0; i < CONFIG_TRUSTFENCE_SRK_WORDS; i++) {
		bank = CONFIG_TRUSTFENCE_SRK_BANK +
		       (i / CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK);
		word = i % CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK;
		ret = fuse_prog(bank, word+CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET,
				src_addr[i]);
		if (ret)
			return ret;
	}

	return 0;
}

#if defined(CONFIG_TRUSTFENCE_SRK_OTP_LOCK_BANK) && \
    defined(CONFIG_TRUSTFENCE_SRK_OTP_LOCK_WORD) && \
    defined(CONFIG_TRUSTFENCE_SRK_OTP_LOCK_OFFSET)
__weak int lock_srk_otp(void)
{
	return fuse_prog(CONFIG_TRUSTFENCE_SRK_OTP_LOCK_BANK,
			 CONFIG_TRUSTFENCE_SRK_OTP_LOCK_WORD,
			 1 << CONFIG_TRUSTFENCE_SRK_OTP_LOCK_OFFSET);
}
#else
__weak int lock_srk_otp(void)	{return 0;}
#endif

#if defined(CONFIG_IMX_HAB)
__weak int revoke_key_index(int i)
{
	u32 val = ((1 << i) & CONFIG_TRUSTFENCE_SRK_REVOKE_MASK) <<
		    CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET;
	return fuse_prog(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			 CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			 val);
}
#endif

#if defined(CONFIG_AHAB_BOOT)
__weak int revoke_keys(void)
{
	return -1;
}
#endif

#if defined(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK) && \
    defined(CONFIG_TRUSTFENCE_SRK_REVOKE_WORD) && \
    defined(CONFIG_TRUSTFENCE_SRK_REVOKE_MASK) && \
    defined(CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET)
__weak int sense_key_status(u32 *val)
{
	if (fuse_sense(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			val))
		return -1;

	*val = (*val >> CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET) &
		CONFIG_TRUSTFENCE_SRK_REVOKE_MASK;

	return 0;
}
#else
__weak int sense_key_status(u32 * val) { return -1; }
#endif

#if defined(CONFIG_TRUSTFENCE_DIRBTDIS_BANK) && \
    defined(CONFIG_TRUSTFENCE_DIRBTDIS_WORD) && \
    defined(CONFIG_TRUSTFENCE_DIRBTDIS_OFFSET)
__weak int disable_ext_mem_boot(void)
{
	return fuse_prog(CONFIG_TRUSTFENCE_DIRBTDIS_BANK,
			 CONFIG_TRUSTFENCE_DIRBTDIS_WORD,
			 1 << CONFIG_TRUSTFENCE_DIRBTDIS_OFFSET);
}
#else
__weak int disable_ext_mem_boot(void) { return -1; }
#endif

#if defined(CONFIG_TRUSTFENCE_CLOSE_BIT_BANK) && \
    defined(CONFIG_TRUSTFENCE_CLOSE_BIT_WORD) && \
    defined(CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET)
__weak int close_device(int confirmed)
{
	hab_rvt_report_status_t *hab_report_status = (hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;
	enum hab_config config = 0;
	enum hab_state state = 0;
	int ret = -1;

	ret = hab_report_status(&config, &state);
	if (ret == HAB_FAILURE) {
		puts("[ERROR]\n There are HAB Events which will prevent the target from booting once closed.\n");
		puts("Run 'hab_status' and check the errors.\n");
		return CMD_RET_FAILURE;
	} else if (ret == HAB_WARNING) {
		if (rng_swtest_status == SW_RNG_TEST_FAILED) {
			puts("[WARNING]\n There are HAB warnings which could prevent the target from booting once closed.\n");
			puts("Run 'hab_status' and check the errors.\n");
			return CMD_RET_FAILURE;
		}
	}

	puts("Before closing the device DIR_BT_DIS will be burned.\n");
	puts("This permanently disables the ability to boot using external memory.\n");
	puts("The SRK_LOCK OTP bit will also be programmed, locking the SRK fields.\n");
	puts("Please confirm the programming of SRK_LOCK, DIR_BT_DIS and SEC_CONFIG[1]\n\n");
	if (!confirmed && !confirm_prog())
		return CMD_RET_FAILURE;

	puts("Programming DIR_BT_DIS eFuse...\n");
	if (disable_ext_mem_boot())
		goto err;
	puts("[OK]\n");

	puts("Programming SRK_LOCK eFuse...\n");
	if (lock_srk_otp())
		goto err;
	puts("[OK]\n");

	puts("Closing device...\n");
	return fuse_prog(CONFIG_TRUSTFENCE_CLOSE_BIT_BANK,
			 CONFIG_TRUSTFENCE_CLOSE_BIT_WORD,
			 1 << CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET);
err:
	return ret;
}
#else
__weak int close_device(int confirmed) { return -1; }
#endif

#ifdef CONFIG_TRUSTFENCE_JTAG
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
#endif /* CONFIG_TRUSTFENCE_JTAG */

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
/*
 * Enable console when selected GPIO level is HIGH
 * @name: Name of the GPIO to read
 *
 * Returns the GPIO level on success and 0 on error.
 */
int console_enable_gpio(const char *name)
{
	struct gpio_desc desc;
	ulong flags = GPIOD_IS_IN;
	int ret = 0;

	if (dm_gpio_lookup_name(name, &desc))
		goto error;

	if (dm_gpio_request(&desc, "Console enable"))
		goto error;

	if (IS_ENABLED(CONFIG_CONSOLE_ENABLE_GPIO_ACTIVE_LOW))
		flags |= GPIOD_ACTIVE_LOW;

	if (dm_gpio_set_dir_flags(&desc, flags))
		goto errfree;

	ret = dm_gpio_get_value(&desc);
errfree:
	dm_gpio_free(NULL, &desc);
error:
	return ret;
}
#endif

#if defined(CONFIG_CONSOLE_ENABLE_PASSPHRASE)
#define INACTIVITY_TIMEOUT		2

/*
 * Grab a passphrase from the console input
 * @secs: Inactivity timeout in seconds
 * @buff: Pointer to passphrase output buffer
 * @len: Length of output buffer
 *
 * Returns zero on success and a negative number on error.
 */
static int console_get_passphrase(int secs, char *buff, int len)
{
	char c;
	uint64_t end_tick, timeout;
	int i;

	/* Set a global timeout to avoid DoS attacks */
	end_tick = get_ticks() + (uint64_t)(secs * get_tbclk());

	/* Set an inactivity timeout */
	timeout = get_ticks() + INACTIVITY_TIMEOUT * get_tbclk();

	*buff = '\0';
	for (i = 0; i < len;) {
		/* Check timeouts */
		uint64_t tks = get_ticks();

		if ((tks > end_tick) || (tks > timeout)) {
			*buff = '\0';
			return -ETIME;
		}

		/* Do not trigger watchdog while typing passphrase */
		schedule();

		if (tstc()) {
			c = getchar();
			i++;
		} else {
			continue;
		}

		switch (c) {
		/* Enter */
		case '\r':
		case '\n':
			*buff = '\0';
			return 0;
		/* nul */
		case '\0':
			continue;
		/* Ctrl-c */
		case 0x03:
			*buff = '\0';
			return -EINVAL;
		default:
			*buff++ = c;
			/* Restart inactivity timeout */
			timeout = get_ticks() + INACTIVITY_TIMEOUT *
				get_tbclk();
		}
	}

	return -EINVAL;
}

#define SHA256_HASH_LEN		32
#define PASSPHRASE_SECS_TIMEOUT	10
#define MAX_PP_LEN			64

/*
 * Returns zero (success) to enable the console, or a non zero otherwise.
 *
 * A sha256 hash is 256 bits (32 bytes) long, and is represented as
 * a 64 digits hex number.
 */
int console_enable_passphrase(void)
{
	char *pp = NULL;
	unsigned char *sha256_pp = NULL;
	unsigned char *pp_hash = NULL;
	int ret = -EINVAL;

	pp = malloc(MAX_PP_LEN + 1);
	if (!pp) {
		debug("Not enough memory for passphrase\n");
		return -ENOMEM;
	}

	ret = console_get_passphrase(PASSPHRASE_SECS_TIMEOUT,
				     pp, MAX_PP_LEN);
	if (ret)
		goto pp_error;

	sha256_pp = malloc(SHA256_HASH_LEN);
	if (!sha256_pp) {
		debug("Not enough memory for passphrase\n");
		ret = -ENOMEM;
		goto pp_error;
	}

	sha256_csum_wd((const unsigned char *)pp, strlen(pp),
		       sha256_pp, CHUNKSZ_SHA256);

	pp_hash = malloc(SHA256_HASH_LEN);
	if (!pp_hash) {
		debug("Not enough memory for passphrase\n");
		ret = -ENOMEM;
		goto pp_hash_error;
	}

	memset(pp_hash, 0, SHA256_HASH_LEN);
	strtohex(CONFIG_CONSOLE_ENABLE_PASSPHRASE_KEY, pp_hash,
		 SHA256_HASH_LEN);
	ret = memcmp(sha256_pp, pp_hash, SHA256_HASH_LEN);

	free(pp_hash);
pp_hash_error:
	free(sha256_pp);
pp_error:
	free(pp);
	return ret;
}
#endif

void fdt_fixup_trustfence(void *fdt) {
	/* Environment encryption is not enabled on open devices */
	if (!imx_hab_is_enabled()) {
		do_fixup_by_path(fdt, "/", "digi,tf-open", NULL, 0, 1);
		return;
	}

#ifdef CONFIG_ENV_AES_CAAM_KEY
	do_fixup_by_path(fdt, "/", "digi,uboot-env,encrypted", NULL, 0, 1);
#endif
	do_fixup_by_path(fdt, "/", "digi,tf-closed", NULL, 0, 1);
}

/* Platform */
__weak int trustfence_status()
{
	hab_rvt_report_status_t *hab_report_status = (hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;
	enum hab_config config = 0;
	enum hab_state state = 0;
	int ret;

	printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted() ?
			"[YES]" : "[NO]");
	puts("* HAB events:\t\t");
	ret = hab_report_status(&config, &state);
	if (ret == HAB_SUCCESS)
		puts("[NO ERRORS]\n");
	else if (ret == HAB_FAILURE)
		puts("[ERRORS PRESENT!]\n");
	else if (ret == HAB_WARNING) {
		if (rng_swtest_status == SW_RNG_TEST_PASSED) {
			puts("[NO ERRORS]\n");
			puts("\n");
			puts("Note: RNG selftest failed, but software test passed\n");
		} else {
			puts("[WARNINGS PRESENT!]\n");
		}
	}

	return 0;
}

static int do_trustfence(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 val[2], addr;
	int ret = -1;
	struct load_fw fwinfo;

	if (argc < 2)
		return CMD_RET_USAGE;

	memset(&fwinfo, 0, sizeof(fwinfo));

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
		fuse_allow_prog(true);
		ret = fuse_prog_srk(addr, val[0]);
		fuse_allow_prog(false);
		if (ret)
			goto err;
		puts("[OK]\n");
	} else if (!strcmp(op, "close")) {
		puts("Checking SRK bank...\n");
		ret = fuse_check_srk();
		if (ret > 0) {
			puts("[ERROR] Burn the SRK OTP bits before "
			     "closing the device.\n");
			return CMD_RET_FAILURE;
		} else if (ret < 0) {
			goto err;
		} else {
			puts("[OK]\n\n");
		}

		if (close_device(confirmed))
			goto err;
		puts("[OK]\n");
	} else if (!strcmp(op, "revoke")) {
#if defined(CONFIG_IMX_HAB)
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
#elif defined(CONFIG_AHAB_BOOT)
		u32 revoke_mask = 0;
		if (get_srk_revoke_mask(&revoke_mask) != CMD_RET_SUCCESS) {
			printf("Failed to get revoke mask.\n");
			return CMD_RET_FAILURE;
		}

		if (revoke_mask) {
			printf("Following keys will be permanently revoked:\n");
			for (int i = 0; i <= CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS; i++) {
				if (revoke_mask & (1 << i))
					printf("   Key %d\n", i);
			}
			if (revoke_mask & (1 << CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS)) {
				puts("Key 3 cannot be revoked. Abort.\n");
				return CMD_RET_FAILURE;
			}
		} else {
			printf("No Keys to be revoked.\n");
			return CMD_RET_FAILURE;
		}

		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		printf("Revoking keys...");
		if (revoke_keys())
			goto err;
		puts("[OK]\n");
		if (!sense_key_status(&val[0])) {
			for (int i = 0; i < CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS; i++) {
				if (val[0] & (1 << i))
					printf("   Key %d revoked\n", i);
			}
		}
#else
		printf("Command not implemented\n");
#endif
	} else if (!strcmp(op, "status")) {
		printf("* SRK fuses:\t\t");
		ret = fuse_check_srk();
		if (ret > 0) {
			printf("[NOT PROGRAMMED]\n");
		} else if (ret == 0) {
			puts("[PROGRAMMED]\n");
			/* Only show revocation status if the SRK fuses are programmed */
			if (!sense_key_status(&val[0])) {
				for (int i = 0; i <= CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS; i++) {
					printf("   Key %d:\t\t", i);
					printf((val[0] & (1 << i) ? "[REVOKED]\n" : "[OK]\n"));
				}
			}
		} else {
			puts("[ERROR]\n");
		}

		printf("* Secure boot:\t\t%s", imx_hab_is_enabled() ?
		       "[CLOSED]\n" : "[OPEN]\n");
		trustfence_status();
#ifdef CONFIG_TRUSTFENCE_UPDATE
	} else if (!strcmp(op, "update")) {
		char cmd_buf[CONFIG_SYS_CBSIZE];
		unsigned long loadaddr = env_get_ulong("loadaddr", 16,
						      CONFIG_SYS_LOAD_ADDR);
		unsigned long uboot_size;
		unsigned long dek_size;
		unsigned long dek_blob_src;
		unsigned long dek_blob_dst;
		unsigned long dek_blob_final_dst;
		unsigned long uboot_start;
		u32 dek_blob_size;
#ifdef CONFIG_AHAB_BOOT
		u32 dek_blob_offset[2];
		unsigned long dek_blob_spl_final_dst;
#endif
#ifdef CONFIG_ARCH_IMX8M
		u32 dek_blob_spl_offset;
		unsigned long dek_blob_spl_final_dst;
		unsigned long fit_dek_blob_size = 96;
#endif
		int generate_dek_blob;
		uint8_t *buffer = NULL;

		argv -= 2 + confirmed;
		argc += 2 + confirmed;

		if (argc < 3)
			return CMD_RET_USAGE;

		/* Get source of firmware file */
		if (get_source(argc, argv, &fwinfo))
			return CMD_RET_FAILURE;

		if (!(
			(fwinfo.src == SRC_MMC && argc >= 5 ) ||
			((fwinfo.src == SRC_TFTP || fwinfo.src == SRC_NFS) && argc >= 4) ||
			(fwinfo.src == SRC_RAM && argc >= 4)
		   ))
			return CMD_RET_USAGE;

		if (fwinfo.src != SRC_RAM) {
			/* If not in RAM, load artifacts to RAM */

			printf("\nLoading encrypted U-Boot image...\n");
			/* Load firmware file to RAM */
			strcpy(fwinfo.loadaddr, "$loadaddr");
			strncpy(fwinfo.filename,
				(fwinfo.src == SRC_MMC) ? argv[4] : argv[3],
				sizeof(fwinfo.filename));
			ret = load_firmware(&fwinfo, NULL);
			if (ret == LDFW_ERROR) {
				printf("Error loading firmware file to RAM\n");
				return CMD_RET_FAILURE;
			}

			uboot_size = env_get_ulong("filesize", 16, 0);
#ifdef CONFIG_AHAB_BOOT
			/* DEK blob will be placed into the Signature Block */
			ret = get_dek_blob_offset(loadaddr, dek_blob_offset);
			if (ret != 0) {
				printf("Error getting the DEK Blob offset (%d)\n", ret);
				return CMD_RET_FAILURE;
			}
			dek_blob_spl_final_dst = loadaddr + dek_blob_offset[0];
			dek_blob_final_dst = loadaddr + dek_blob_offset[1];
#else
#ifdef CONFIG_ARCH_IMX8M
			ret = get_dek_blob_offset((void *)loadaddr, &dek_blob_spl_offset);
			if (ret != 0) {
				printf("Error getting the DEK Blob offset (%d)\n", ret);
				return CMD_RET_FAILURE;
			}
			/* Get SPL dek blob memory locations */
			dek_blob_spl_final_dst = loadaddr + dek_blob_spl_offset;
			/* DEK blob will be inserted in the dummy DEK location of the U-Boot image */
			dek_blob_final_dst = loadaddr + uboot_size - fit_dek_blob_size;
#else
			/* DEK blob will be directly appended to the U-Boot image */
			dek_blob_final_dst = loadaddr + uboot_size;
#endif // CONFIG_ARCH_IMX8M
#endif
			/*
			 * for the DEK blob source (DEK in plain text) we use the
			 * first 0x100 aligned memory address
			 */
			dek_blob_src = ((loadaddr + uboot_size) & 0xFFFFFF00) + 0x100;

			/*
			 * DEK destination also needs to be 0x100 aligned. Leave
			 * 0x400 = 1KiB to fit the DEK source
			 */
			dek_blob_dst = dek_blob_src + 0x400;

			debug("loadaddr:           0x%lx\n", loadaddr);
			debug("uboot_size:         0x%lx\n", uboot_size);
			debug("dek_blob_src:       0x%lx\n", dek_blob_src);
			debug("dek_blob_dst:       0x%lx\n", dek_blob_dst);
			debug("dek_blob_final_dst: 0x%lx\n", dek_blob_final_dst);
			debug("U-Boot:             [0x%lx,\t0x%lx]\n", loadaddr, loadaddr + uboot_size);

			printf("\nLoading Data Encryption Key...\n");
			if ((fwinfo.src == SRC_TFTP || fwinfo.src == SRC_NAND) &&
			    argc >= 5) {
				sprintf(cmd_buf, "%s 0x%lx %s", argv[2], dek_blob_src,
					argv[4]);
				generate_dek_blob = 1;
			} else if (fwinfo.src == SRC_MMC && argc >= 6) {
				sprintf(cmd_buf, "load mmc %s 0x%lx %s", argv[3],
					dek_blob_src, argv[5]);
				generate_dek_blob = 1;
			} else {
				generate_dek_blob = 0;
			}

			/* To generate the DEK blob, first load the DEK to RAM */
			if (generate_dek_blob) {
				debug("\tCommand: %s\n", cmd_buf);
				if (run_command(cmd_buf, 0))
					return CMD_RET_FAILURE;
				dek_size = env_get_ulong("filesize", 16, 0);
			}
		} else {
			/*
			 * If artifacts are in RAM, set up an aligned
			 * buffer to work with them.
			 */
			uboot_start = simple_strtoul(argv[3], NULL, 16);
			uboot_size = simple_strtoul(argv[4], NULL, 16);
			unsigned long dek_start = argc >= 5 ? simple_strtoul(argv[5], NULL, 16) : 0;
			dek_size = argc >= 6 ? simple_strtoul(argv[6], NULL, 16) : 0;

			/*
			 * This buffer will hold U-Boot, DEK and DEK blob. As
			 * this function progresses, it will hold the
			 * following:
			 *
			 * | <U-Boot> | <DEK> | <blank>
			 * | <U-Boot> | <DEK> | <DEK blob>
			 * | <U-Boot> <DEK-blob>
			 *
			 * Note: '|' represents the start of a
			 * DMA-aligned region.
			 */
			buffer = malloc_cache_aligned(DMA_ALIGN_UP(uboot_size) + 2 * DMA_ALIGN_UP(MAX_DEK_BLOB_SIZE));
			if (!buffer) {
				printf("Out of memory!\n");
				return CMD_RET_FAILURE;
			}

			dek_blob_src = (uintptr_t) (buffer + DMA_ALIGN_UP(uboot_size));
			dek_blob_dst = (uintptr_t) (buffer + DMA_ALIGN_UP(uboot_size) + DMA_ALIGN_UP(MAX_DEK_BLOB_SIZE));
#ifdef CONFIG_AHAB_BOOT
			/* DEK blob will be placed into the Signature Block */
			ret = get_dek_blob_offset(uboot_start, dek_blob_offset);
			if (ret != 0) {
				printf("Error getting the DEK Blob offset (%d)\n", ret);
				return CMD_RET_FAILURE;
			}
			dek_blob_spl_final_dst = (uintptr_t) (buffer + dek_blob_offset[0]);
			dek_blob_final_dst = (uintptr_t) (buffer + dek_blob_offset[1]);
#else
#ifdef CONFIG_SPL
			/* DEK blob will be directly inserted into the U-Boot image */
			ret = get_dek_blob_offset((void *)uboot_start, &dek_blob_spl_offset);
			if (ret != 0) {
				printf("Error getting the SPL DEK Blob offset (%d)\n", ret);
				return CMD_RET_FAILURE;
			}
			dek_blob_spl_final_dst = (uintptr_t) (buffer + dek_blob_spl_offset);
			/* DEK blob will be added to SPL dek blob location */
                        dek_blob_final_dst = (uintptr_t) (buffer + uboot_size - fit_dek_blob_size);
			debug("Buffer:             [0x%p,\t0x%p]\n", buffer, buffer + DMA_ALIGN_UP(uboot_size) + 4 * DMA_ALIGN_UP(MAX_DEK_BLOB_SIZE));
#else
			/* DEK blob will be directly appended to the U-Boot image */
			dek_blob_final_dst = (uintptr_t) (buffer + uboot_size);
			debug("Buffer:             [0x%p,\t0x%p]\n", buffer, buffer + DMA_ALIGN_UP(uboot_size) + 2 * DMA_ALIGN_UP(MAX_DEK_BLOB_SIZE));
#endif // CONFIG_SPL
#endif
			debug("dek_blob_src:       0x%lx\n", dek_blob_src);
			debug("dek_blob_dst:       0x%lx\n", dek_blob_dst);
			debug("dek_blob_final_dst: 0x%lx\n", dek_blob_final_dst);
			debug("ARCH_DMA_MINALIGN:  0x%x\n", ARCH_DMA_MINALIGN);
			debug("dma_(dek_blob):     0x%x\n", DMA_ALIGN_UP(MAX_DEK_BLOB_SIZE));
			debug("U-Boot:             [0x%p,\t0x%p]\n", buffer, buffer + uboot_size);

			memcpy(buffer, (void *)uboot_start, uboot_size);

			if (dek_start > 0 && dek_size > 0) {
				memcpy((void *)dek_blob_src, (void *)dek_start, dek_size);
				generate_dek_blob = 1;
			} else {
				generate_dek_blob = 0;
			}

			loadaddr = (uintptr_t) buffer;
		}

		/*
		 * The following if-else block is in charge of appending the
		 * DEK blob to 'dek_blob_final_dst'.
		 */
		if (generate_dek_blob) {
			/*
			 * If generate_dek_blob, then the DEK blob is generated from
			 * the DEK (at dek_blob_src) and then copied into its final
			 * destination.
			 */
			printf("\nGenerating DEK blob...\n");
			/* dek_blob takes size in bits */
			sprintf(cmd_buf, "dek_blob 0x%lx 0x%lx 0x%lx",
				dek_blob_src, dek_blob_dst, dek_size * 8);
			debug("\tCommand: %s\n", cmd_buf);
			if (run_command(cmd_buf, 0)) {
				ret = CMD_RET_FAILURE;
				goto tf_update_out;
			}

#ifdef CONFIG_AHAB_BOOT
			get_dek_blob_size(dek_blob_dst, &dek_blob_size);
#else
			/*
			 * Set dek_size to the size of the DEK blob, that is:
			 * header (8 bytes) + random AES-256 key (32 bytes)
			 * + DEK ('dek_size' bytes) + MAC (16 bytes)
			 */
			dek_blob_size = 8 + 32 + dek_size + 16;
#endif
			/* Copy DEK blob to its final destination */
                        memcpy((void *)dek_blob_final_dst, (void *)dek_blob_dst,
                                dek_blob_size);
#if defined(CONFIG_SPL)
			/* Copy SPL DEK blob to its final destination */
			memcpy((void *)dek_blob_spl_final_dst, (void *)dek_blob_dst,
				dek_blob_size);
#endif
		} else {
			/* If !generate_dek_blob, then the DEK blob from the running
			 * U-Boot is recovered and copied into its final
			 * destination. (This fails if the running U-Boot does not
			 * include a DEK)
			 */
#if defined(CONFIG_SPL)
			if (get_dek_blob(dek_blob_spl_final_dst, &dek_blob_size)) {
				printf("Current U-Boot does not contain an SPL DEK, and a new SPL DEK was not provided\n");
				ret = CMD_RET_FAILURE;
				goto tf_update_out;
			}
#endif
			if (get_dek_blob(dek_blob_final_dst, &dek_blob_size)) {
				printf("Current U-Boot does not contain a DEK, and a new DEK was not provided\n");
				ret = CMD_RET_FAILURE;
				goto tf_update_out;
			}
			printf("Using current DEK\n");
		}

		printf("\nFlashing U-Boot partition...\n");
#ifdef CONFIG_AHAB_BOOT
		sprintf(cmd_buf, "update uboot ram 0x%lx 0x%lx",
			loadaddr, uboot_size);
#else
#ifdef CONFIG_SPL
		sprintf(cmd_buf, "update uboot ram 0x%lx 0x%lx",
                        loadaddr,
                        uboot_size);
#else
		sprintf(cmd_buf, "update uboot ram 0x%lx 0x%lx",
			loadaddr,
			dek_blob_final_dst + dek_blob_size - loadaddr);
#endif // CONFIG_SPL
#endif
		debug("\tCommand: %s\n", cmd_buf);
		env_set("forced_update", "y");
		ret = run_command(cmd_buf, 0);
		env_set("forced_update", "n");

tf_update_out:
		free(buffer);
		if (ret) {
			printf("Operation failed!\n");
			return CMD_RET_FAILURE;
		}
#endif /* CONFIG_TRUSTFENCE_UPDATE */
#ifdef CONFIG_TRUSTFENCE_JTAG
	} else if (!strcmp(op, "jtag")) {
		char jtag_op[15];
		int i;
		u32 bank = CONFIG_TRUSTFENCE_JTAG_MODE_BANK;
		u32 word = CONFIG_TRUSTFENCE_JTAG_MODE_START_WORD;
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

			snprintf(jtag_op, sizeof(jtag_op), "%s", argv[1]);

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

			snprintf(jtag_op, sizeof(jtag_op), "%s", argv[1]);

			printf("Overriding Secure JTAG mode... ");
			if (!strcmp(jtag_op, "secure")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_ENABLE_SECURE_JTAG_MODE);
			} else if (!strcmp(jtag_op, "disable-debug")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_DISABLE_DEBUG);
			} else if (!strcmp(jtag_op, "disable-jtag")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_DISABLE_JTAG);
			} else if (!strcmp(jtag_op, "enable-jtag")) {
				ret = fuse_override(bank, word, TRUSTFENCE_JTAG_ENABLE_JTAG);
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
#endif /* CONFIG_TRUSTFENCE_JTAG */
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
#if defined(CONFIG_IMX_HAB)
	"trustfence revoke [-y] <key index> - revoke one Super Root Key (PERMANENT)\n"
#elif defined(CONFIG_AHAB_BOOT)
	"trustfence revoke [-y] - revoke one or more Super Root Keys as per the SRK_REVOKE_MASK given at build time in the CSF (PERMANENT)\n"
#endif
#ifdef CONFIG_TRUSTFENCE_UPDATE
	"trustfence update <source> [extra-args...]\n"
	" Description: flash an encrypted U-Boot image.\n"
	" Arguments:\n"
	"\n\t- <source>: tftp|nfs|mmc|ram\n"
	"\n\tsource=tftp|nfs -> <uboot_file> [<dek_file>]\n"
	"\t\t - <uboot_file>: name of the encrypted uboot image\n"
	"\t\t - <dek_file>: name of the Data Encryption Key (DEK) in plain text\n"
	"\n\tsource=mmc -> <dev:part> <uboot_file> [<dek_file>]\n"
	"\t\t - <dev:part>: number of device and partition\n"
	"\t\t - <uboot_file>: name of the encrypted uboot image\n"
	"\t\t - <dek_file>: name of the Data Encryption Key (DEK) in plain text.\n"
	"\n\tsource=ram -> <uboot_start> <uboot_size> [<dek_start> <dek_size>]\n"
	"\t\t - <uboot_start>: U-Boot binary memory address\n"
	"\t\t - <uboot_size>: size of U-Boot binary (in bytes)\n"
	"\t\t - <dek_start>: Data Encryption Key (DEK) memory address\n"
	"\t\t - <dek_size>: size of DEK (in bytes)\n"
	"\n"
	" Note: the DEK arguments are optional if the current U-Boot is encrypted.\n"
	"       If skipped, the current DEK will be re-used\n"
	"\n"
#endif /* CONFIG_TRUSTFENCE_UPDATE */
#ifdef CONFIG_TRUSTFENCE_JTAG
	"WARNING: These commands (except 'status' and 'update') burn the eFuses.\n"
	"They are irreversible and could brick your device.\n"
	"Make sure you know what you do before playing with this command.\n"
	"\n"
	"-- SECURE JTAG --\n"
	"trustfence jtag read - read Secure JTAG mode from shadow registers\n"
	"trustfence jtag read_key - read Secure JTAG response key from shadow registers\n"
	"trustfence jtag [-y] prog <mode> - program Secure JTAG mode <mode> (PERMANENT). <mode> can be one of:\n"
	"    secure - Secure JTAG mode (debugging only possible by providing the key "
	"burned in the e-fuses)\n"
	"    disable-debug - JTAG debugging disabled (only boundary-scan possible)\n"
	"    disable-jtag - JTAG port disabled (no JTAG operations allowed)\n"
	"trustfence jtag [-y] prog_key <high_word> <low_word> - program response key (PERMANENT)\n"
#if defined CONFIG_TRUSTFENCE_JTAG_OVERRIDE
	"trustfence jtag override <mode> - override Secure JTAG mode <mode>. <mode> can be one of:\n"
	"    secure - Secure JTAG mode (debugging only possible by providing the key "
	"burned in the e-fuses)\n"
	"    disable-debug - JTAG debugging disabled (only boundary-scan possible)\n"
	"    disable-jtag - JTAG port disabled (no JTAG operations allowed)\n"
	"    enable-jtag - JTAG port enabled (JTAG operations allowed)\n"
	"trustfence jtag override_key <high_word> <low_word> - override response key\n"
#endif
	"trustfence jtag [-y] lock - lock Secure JTAG mode and disable JTAG interface "
				"OTP bits (PERMANENT)\n"
	"trustfence jtag [-y] lock_key - lock Secure JTAG key OTP bits (PERMANENT)\n"
#endif /* CONFIG_TRUSTFENCE_JTAG */
);
