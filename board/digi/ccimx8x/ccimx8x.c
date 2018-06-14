/*
 * Copyright (C) 2018 Digi International, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fsl_esdhc.h>
#include <i2c.h>
#include <otf_update.h>

#include <asm/imx-common/sci/sci.h>
#include <asm/imx-common/boot_mode.h>
#include "../../freescale/common/tcpc.h"

#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "ccimx8x.h"

DECLARE_GLOBAL_DATA_PTR;

#define MCA_CC8X_DEVICE_ID_VAL		0x4A

#ifdef CONFIG_FSL_ESDHC

static struct blk_desc *mmc_dev;
static int mmc_dev_index = -1;

int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 1;	/* index of USDHC2 (SD card) */
	case MMC1_BOOT:
		return 0;	/* index of USDHC1 (eMMC) */
	default:
		/* return default value otherwise */
		return CONFIG_SYS_MMC_ENV_DEV;
	}
}

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int board_has_emmc(void)
{
	return 1;
}

static int write_chunk(struct mmc *mmc, otf_data_t *otfd, unsigned int dstblk,
			unsigned int chunklen)
{
	int sectors;
	unsigned long written, read, verifyaddr;

	printf("\nWriting chunk...");
	/* Check WP */
	if (mmc_getwp(mmc) == 1) {
		printf("[Error]: card is write protected!\n");
		return -1;
	}

	/* We can only write whole sectors (multiples of mmc_dev->blksz bytes)
	 * so we need to check if the chunk is a whole multiple or else add 1
	 */
	sectors = chunklen / mmc_dev->blksz;
	if (chunklen % mmc_dev->blksz)
		sectors++;

	/* Check if chunk fits */
	if (sectors + dstblk > otfd->part->start + otfd->part->size) {
		printf("[Error]: length of data exceeds partition size\n");
		return -1;
	}

	/* Write chunklen bytes of chunk to media */
	debug("writing chunk of 0x%x bytes (0x%x sectors) "
		"from 0x%lx to block 0x%x\n",
		chunklen, sectors, otfd->loadaddr, dstblk);
	written = mmc->block_dev.block_write(mmc_dev, dstblk, sectors,
					     (const void *)otfd->loadaddr);
	if (written != sectors) {
		printf("[Error]: written sectors != sectors to write\n");
		return -1;
	}
	printf("[OK]\n");

	/* Verify written chunk if $loadaddr + chunk size does not overlap
	 * $verifyaddr (where the read-back copy will be placed)
	 */
	verifyaddr = getenv_ulong("verifyaddr", 16, 0);
	if (otfd->loadaddr + sectors * mmc_dev->blksz < verifyaddr) {
		/* Read back data... */
		printf("Reading back chunk...");
		read = mmc->block_dev.block_read(mmc_dev, dstblk, sectors,
						 (void *)verifyaddr);
		if (read != sectors) {
			printf("[Error]: read sectors != sectors to read\n");
			return -1;
		}
		printf("[OK]\n");
		/* ...then compare */
		printf("Verifying chunk...");
		if (memcmp((const void *)otfd->loadaddr,
			      (const void *)verifyaddr,
			      sectors * mmc_dev->blksz)) {
			printf("[Error]\n");
			return -1;
		} else {
			printf("[OK]\n");
			return 0;
		}
	} else {
		printf("[Warning]: Cannot verify chunk. "
			"It overlaps $verifyaddr!\n");
		return 0;
	}
}

/* writes a chunk of data from RAM to main storage media (eMMC) */
int board_update_chunk(otf_data_t *otfd)
{
	static unsigned int chunk_len = 0;
	static unsigned int dstblk = 0;
	struct mmc *mmc;

	if (mmc_dev_index == -1)
		mmc_dev_index = getenv_ulong("mmcdev", 16,
					     mmc_get_bootdevindex());
	mmc = find_mmc_device(mmc_dev_index);
	if (NULL == mmc)
		return -1;
	mmc_dev = mmc_get_blk_desc(mmc);
	if (NULL == mmc_dev) {
		printf("ERROR: failed to get block descriptor for MMC device\n");
		return -1;
	}

	/*
	 * There are two variants:
	 *  - otfd.buf == NULL
	 *  	In this case, the data is already waiting on the correct
	 *  	address in RAM, waiting to be written to the media.
	 *  - otfd.buf != NULL
	 *  	In this case, the data is on the buffer and must still be
	 *  	copied to an address in RAM, before it is written to media.
	 */
	if (otfd->buf && otfd->len) {
		/*
		 * If data is in the otfd->buf buffer, copy it to the loadaddr
		 * in RAM until we have a chunk that is at least as large as
		 * CONFIG_OTF_CHUNK, to write it to media.
		 */
		memcpy((void *)(otfd->loadaddr + otfd->offset), otfd->buf,
		       otfd->len);
	}

	/* Initialize dstblk and local variables */
	if (otfd->flags & OTF_FLAG_INIT) {
		chunk_len = 0;
		dstblk = otfd->part->start;
		otfd->flags &= ~OTF_FLAG_INIT;
	}
	chunk_len += otfd->len;

	/* The flush flag is set when the download process has finished
	 * meaning we must write the remaining bytes in RAM to the storage
	 * media. After this, we must quit the function. */
	if (otfd->flags & OTF_FLAG_FLUSH) {
		/* Write chunk with remaining bytes */
		if (chunk_len) {
			if (write_chunk(mmc, otfd, dstblk, chunk_len))
				return -1;
		}
		/* Reset all static variables if offset == 0 (starting chunk) */
		chunk_len = 0;
		dstblk = 0;
		return 0;
	}

	if (chunk_len >= CONFIG_OTF_CHUNK) {
		unsigned int remaining;
		/* We have CONFIG_OTF_CHUNK (or more) bytes in RAM.
		 * Let's proceed to write as many as multiples of blksz
		 * as possible.
		 */
		remaining = chunk_len % mmc_dev->blksz;
		chunk_len -= remaining;	/* chunk_len is now multiple of blksz */

		if (write_chunk(mmc, otfd, dstblk, chunk_len))
			return -1;

		/* increment destiny block */
		dstblk += (chunk_len / mmc_dev->blksz);
		/* copy excess of bytes from previous chunk to offset 0 */
		if (remaining) {
			memcpy((void *)otfd->loadaddr,
			       (void *)(otfd->loadaddr + chunk_len),
			       remaining);
			debug("Copying excess of %d bytes to offset 0\n",
			      remaining);
		}
		/* reset chunk_len to excess of bytes from previous chunk
		 * (or zero, if that's the case) */
		chunk_len = remaining;
	}
	/* Set otfd offset pointer to offset in RAM where new bytes would
	 * be written. This offset may be reused by caller */
	otfd->offset = chunk_len;

	return 0;
}

#endif /* CONFIG_FSL_ESDHC */

static void mca_init(void)
{
	unsigned char devid = 0;
	unsigned char hwver;
	unsigned char fwver[2];
	int ret, fwver_ret;

#ifdef CONFIG_DM_I2C
	struct udevice *bus, *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, CONFIG_MCA_I2C_BUS, &bus);
	if (ret) {
		printf("ERROR: getting %d bus for MCA\n", CONFIG_MCA_I2C_BUS);
		return;
	}

	ret = dm_i2c_probe(bus, CONFIG_MCA_I2C_ADDR, 0, &dev);
	if (ret) {
		printf("ERROR: can't find MCA at address %x\n",
		       CONFIG_MCA_I2C_ADDR);
		return;
	}

	ret = i2c_set_chip_offset_len(dev, 2);
	if (ret)
		printf("ERROR: setting address len to 2 for MCA\n");
#endif
	ret = mca_read_reg(MCA_DEVICE_ID, &devid);
	if (devid != MCA_CC8X_DEVICE_ID_VAL) {
		printf("MCA: invalid MCA DEVICE ID (0x%02x)\n", devid);
		return;
	}

	ret = mca_read_reg(MCA_HW_VER, &hwver);
	fwver_ret = mca_bulk_read(MCA_FW_VER_L, fwver, 2);

	printf("MCA:   HW_VER=");
	if (ret)
		printf("??");
	else
		printf("%d", hwver);

	printf("  FW_VER=");
	if (fwver_ret)
		printf("??");
	else
		printf("%d.%02d %s", fwver[1] & 0x7f, fwver[0],
		       fwver[1] & 0x80 ? "(alpha)" : "");

	printf("\n");
}

int ccimx8x_late_init(void)
{
	/*
	 * FIXME: the MCA shoudld be initialized earlier, but we depend on the
	 * I2C bus being ready. This should be revisited in future and it is
	 * likely that we will have to initialize the i2c earlier (for instance
	 * in the early init function, and then initialize the MCA.
	 */
	mca_init();

	return 0;
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(ulong addr)
{
	puts("SCI reboot request");
	sc_pm_reboot(SC_IPC_CH, SC_PM_RESET_TYPE_COLD);
	while (1)
		putc('.');
}

/*
 * Use the reset_misc hook to customize the reset implementation. This function
 * is invoked before the standard reset_cpu().
 * On the CC8X we want to perfrom the reset through the MCA which may, depending
 * on the configuration, assert the POR_B line or perform a power cycle of the
 * system.
 */
void reset_misc(void)
{
#if 0
	/*
	 * If a bmode_reset was flagged, do not reset through the MCA, which
	 * would otherwise power-cycle the CPU.
	 */
	if (bmode_reset)
		return;
#endif
	mca_reset();

	mdelay(1);

#ifdef CONFIG_PSCI_RESET
	psci_system_reset();
	mdelay(1);
#endif
}

void detail_board_ddr_info(void)
{
	puts("\nDDR    ");
}
