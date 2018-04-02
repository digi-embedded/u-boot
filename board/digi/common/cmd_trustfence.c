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
#include <fsl_sec.h>
#include <fuse.h>
#include <asm/imx-common/hab.h>
#include <linux/errno.h>
#include "helper.h"
#include <u-boot/sha256.h>
#include <watchdog.h>
#include <malloc.h>
#include <mapmem.h>
#ifdef CONFIG_CONSOLE_ENABLE_GPIO
#include <asm/gpio.h>
#endif
#include "trustfence.h"
#include <u-boot/md5.h>
#include <fsl_caam.h>

#define UBOOT_HEADER_SIZE	0xC00
#define UBOOT_START_ADDR	(CONFIG_SYS_TEXT_BASE - UBOOT_HEADER_SIZE)

/* Location of the CSF pointer within the Image Vector Table */
#define CSF_IVT_WORD_OFFSET	6
#define BLOB_DEK_OFFSET		0x100

/*
 * If CONFIG_CSF_SIZE is undefined, assume 0x4000. This value will be used
 * in the signing script.
 */
#ifndef CONFIG_CSF_SIZE
#define CONFIG_CSF_SIZE 0x4000
#endif

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
static int get_dek_blob(char *output, u32 *size) {
	u32 *csf_addr = (u32 *)UBOOT_START_ADDR + CSF_IVT_WORD_OFFSET;

	if (*csf_addr) {
		int blob_size = MAX_DEK_BLOB_SIZE;
		uint8_t *dek_blob = (uint8_t *)(*csf_addr + CONFIG_CSF_SIZE - blob_size);

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

int is_uboot_encrypted() {
	char dek_blob[MAX_DEK_BLOB_SIZE];
	u32 dek_blob_size;

	/* U-Boot is encrypted if and only if get_dek_blob does not fail */
	return !get_dek_blob(dek_blob, &dek_blob_size);
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
 * Function:    secure_memzero
 * Description: secure memzero that is not optimized out by the compiler
 */
static void secure_memzero(void *buf, size_t len)
{
	volatile uint8_t *p = (volatile uint8_t *)buf;

	while (len--)
		*p++ = 0;
}

static int validate_tf_key(u32 addr, uint hwpart)
{
	unsigned char *data = NULL;
	unsigned char *readbuf = NULL;
	unsigned char key_modifier[16] = {0};
	int ret = -1;
	size_t block_size;
	size_t key_size = 32;

	block_size = media_get_block_size();
	if (!block_size)
		goto exit;

	data = malloc(block_size);
	if (!data)
		goto exit;

	readbuf = malloc(block_size);
	if (!readbuf)
		goto out_free;

	if (media_read_block(addr, readbuf, hwpart))
		goto out_free;

	if (get_trustfence_key_modifier(key_modifier))
		goto out_free;

	caam_open();
	ret = caam_decap_blob(data, readbuf, key_modifier, key_size);
	/* Always empty the data in a secure way */
	secure_memzero(data, key_size);
out_free:
	free(readbuf);
	free(data);
exit:
	return ret;
}

static int move_key(u32 orig, uint orig_hwpart, u32 dest, uint dest_hwpart)
{
	unsigned char *readbuf = NULL;
	int ret = -1;
	size_t block_size;

	block_size = media_get_block_size();
	if (!block_size)
		goto exit;

	readbuf = malloc(block_size);
	if (!readbuf)
		goto exit;

	if (media_read_block(orig, readbuf, orig_hwpart))
		goto out_free;

	if (media_write_block(dest, readbuf, dest_hwpart))
		goto out_free;

	ret = 0;
out_free:
	free(readbuf);
exit:
	return ret;
}

/*
 * At the beginning we were storing the TF RootFS Key in the 'environment'
 * partition. It could be that we will override that Key,
 * so we need to move it to 'safe' partition.
 */
void migrate_filesystem_key(void)
{
	ulong safe_addr, env_addr;
	uint env_hwpart = get_env_hwpart();
	uint safe_hwpart = 0;

	if (get_partition_offset("safe", &safe_addr))
		return;

	/* If the key in the 'safe' partition is valid, do nothing */
	if (validate_tf_key(safe_addr, safe_hwpart) == 0)
		return;

	/* Get the addr for the TF RootFS Key in the 'environment' partition */
	env_addr = get_filesystem_key_offset();
	if (env_addr <= 0)
		return;

	/* If there is not valid key, do nothing */
	if (validate_tf_key(env_addr, env_hwpart) != 0)
		return;

	if (!media_block_is_empty(safe_addr, safe_hwpart)) {
		printf("[WARNING] There is a filesystem encryption key in the environment partition.\n"
			"          U-Boot needs to move it to the first block of the 'safe' partition,\n"
			"          where TrustFence expects it but this partition is currently not empty.\n"
			"          Erase the 'safe' partition to let U-Boot move the key\n"
			"          and to remove this warning.\n"
		);
		return;
	}

	/* Move the key from 'environment' partition to 'safe' partition */
	if (!move_key(env_addr, env_hwpart, safe_addr, safe_hwpart)) {
		/*
		 * Check that key was really well copied. Do not erase the key
		 * from the environment if it was well copied to keep compatibility.
		 */
		if (validate_tf_key(safe_addr, safe_hwpart) != 0) {
			/*
			 * Critical scenario, the TF RootFS Key was incorrectly copied.
			 * We still have it in the 'environment' partition, so clear
			 * the data in 'safe' and the key will be moved in the next boot.
			 */
			media_erase_fskey(safe_addr, safe_hwpart);
			printf("[ERROR] TF RootFS Key could not be copied to 'safe' partition\n");
		}
	} else {
		printf("[ERROR] TF RootFS Key could not be copied to 'safe' partition\n");
	}
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
	ulong loadaddr = getenv_ulong("loadaddr", 16, load_addr);
	void *dek_blob_dst = (void *)(loadaddr - BLOB_DEK_OFFSET);
	u32 dek_size;

	get_dek_blob(dek_blob_dst, &dek_size);
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

	for (i = 0; i < CONFIG_TRUSTFENCE_SRK_WORDS; i++) {
		if (fuse_sense(CONFIG_TRUSTFENCE_SRK_BANK, i, &val))
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
	uint32_t *src_addr = map_sysmem(addr, size);

	if (size != CONFIG_TRUSTFENCE_SRK_WORDS * 4) {
		puts("Bad size\n");
		return -1;
	}

	for (i = 0; i < CONFIG_TRUSTFENCE_SRK_WORDS; i++) {
		ret = fuse_prog(CONFIG_TRUSTFENCE_SRK_BANK, i, src_addr[i]);
		if (ret)
			return ret;
	}

	return 0;
}

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

__weak int disable_ext_mem_boot(void)
{
	return fuse_prog(CONFIG_TRUSTFENCE_DIRBTDIS_BANK,
			 CONFIG_TRUSTFENCE_DIRBTDIS_WORD,
			 1 << CONFIG_TRUSTFENCE_DIRBTDIS_OFFSET);
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

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
int console_enable_gpio(int gpio)
{
	int ret = 0;

	if (gpio_request(gpio, "console_enable") == 0) {
		ret = gpio_get_value(gpio);
		gpio_free(gpio);
	}

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
		WATCHDOG_RESET();

		if (tstc()) {
			c = getc();
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
	unsigned long *pp_hash = NULL;
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
		 SHA256_HASH_LEN/sizeof(unsigned long));
	ret = memcmp(sha256_pp, pp_hash, SHA256_HASH_LEN);

	free(pp_hash);
pp_hash_error:
	free(sha256_pp);
pp_error:
	free(pp);
	return ret;
}
#endif

#ifdef CONFIG_ENV_AES_KEY
/*
 * CONFIG_ENV_AES_KEY is a 128 bits (16 bytes) AES key, represented as
 * 32 hexadecimal characters.
 */
unsigned long key[4];

uint8_t *env_aes_cbc_get_key(void)
{
	if (strlen(CONFIG_ENV_AES_KEY) != 32) {
		puts("[ERROR] Wrong CONFIG_ENV_AES_KEY size (should be 128 bits)\n");
		return NULL;
	}

	strtohex(CONFIG_ENV_AES_KEY, key,
		 sizeof(key) / sizeof(unsigned long));

	return (uint8_t *)key;
}
#endif

#ifdef CONFIG_ENV_AES_CAAM_KEY
#include <fdt_support.h>

void fdt_fixup_trustfence(void *fdt) {
	/* Environment encryption is not enabled on open devices */
	if (!is_hab_enabled())
		return;

	do_fixup_by_path(fdt, "/", "digi,uboot-env,encrypted", NULL, 0, 1);
}
#endif

static int do_trustfence(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 bank = CONFIG_TRUSTFENCE_JTAG_MODE_BANK;
	u32 word = CONFIG_TRUSTFENCE_JTAG_MODE_START_WORD;
	u32 val[2], addr;
	char jtag_op[15];
	int ret = -1, i = 0;
	hab_rvt_report_status_t *hab_report_status = hab_rvt_report_status_p;
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
		if (fuse_prog_srk(addr, val[0]))
			goto err;
		puts("[OK]\n");
	} else if (!strcmp(op, "close")) {
		enum hab_config config = 0;
		enum hab_state state = 0;

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

		if (hab_report_status(&config, &state) != HAB_SUCCESS) {
			puts("[ERROR]\n There are HAB Events which will "
			     "prevent the target from booting once closed.\n");
			puts("Run 'hab_status' and check the errors.\n");
			return CMD_RET_FAILURE;
		}

		puts("Before closing the device DIR_BT_DIS will be burned.\n");
		puts("This permanently disables the ability to boot using external memory.\n");
		puts("Please confirm the programming of DIR_BT_DIS and SEC_CONFIG[1]\n\n");
		if (!confirmed && !confirm_prog())
			return CMD_RET_FAILURE;

		puts("Programming DIR_BT_DIS eFuse...\n");
		if (disable_ext_mem_boot())
			goto err;
		puts("[OK]\n");

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

		printf("* SRK fuses:\t\t");
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
			printf("   Key %d:\t\t", key_index);
			printf((val[0] & (1 << key_index) ?
			       "[REVOKED]\n" : "[OK]\n"));
		}
		printf("   Key %d:\t\t[OK]\n", CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS);

		printf("* Secure boot:\t\t%s", is_hab_enabled() ?
		       "[CLOSED]\n" : "[OPEN]\n");

		printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted() ?
			"[YES]" : "[NO]");

		puts("* HAB events:\t\t");
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
		int generate_dek_blob;

		argv -= 2 + confirmed;
		argc += 2 + confirmed;

		if (argc < 3)
			return CMD_RET_USAGE;

		/* Get source of firmware file */
		if (get_source(argc, argv, &fwinfo))
			return CMD_RET_FAILURE;

		if (!((fwinfo.src == SRC_MMC && argc >= 5 ) ||
		     ((fwinfo.src == SRC_TFTP || fwinfo.src == SRC_NFS) &&
			argc >= 4)))
			return CMD_RET_USAGE;

		printf("\nLoading encrypted U-Boot image...\n");
		/* Load firmware file to RAM */
		fwinfo.loadaddr = "$loadaddr";
		fwinfo.filename = (fwinfo.src == SRC_MMC) ? argv[4] : argv[3];
		ret = load_firmware(&fwinfo);
		if (ret == LDFW_ERROR) {
			printf("Error loading firmware file to RAM\n");
			return CMD_RET_FAILURE;
		}

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
			u32 dek_blob_size;

			if (get_dek_blob((void *)dek_blob_final_dst, &dek_blob_size)) {
				printf("Current U-Boot does not contain a DEK, and a new DEK was not provided\n");
				return CMD_RET_FAILURE;
			}
			printf("Using current DEK\n");
			filesize = dek_blob_size;
			generate_dek_blob = 0;
		}

		if (generate_dek_blob) {
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
		}

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
	"\t\t - <dek_file>: name of the Data Encryption Key (DEK) in plain text.\n"
	"\t\t               If the current U-Boot is encrypted, this parameter can\n"
	"\t\t               be skipped and the current DEK will be used\n"
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
	"    enable-jtag - JTAG port enabled (JTAG operations allowed)\n"
	"trustfence jtag override_key <high_word> <low_word> - override response key\n"
#endif
	"trustfence jtag lock [-y] - lock Secure JTAG mode and disable JTAG interface "
				"OTP bits (PERMANENT)\n"
	"trustfence jtag lock_key [-y] - lock Secure JTAG key OTP bits (PERMANENT)\n"
);
