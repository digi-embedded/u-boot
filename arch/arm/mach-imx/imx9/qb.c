// SPDX-License-Identifier: GPL-2.0+
/**
 * Copyright 2024-2026 NXP
 */
#include <dm/device-internal.h>
#include <errno.h>
#include <imx_container.h>
#include <linux/bitfield.h>
#include <mmc.h>
#include <spi_flash.h>
#include <spl.h>
#include <stdlib.h>

#include <asm/arch/ddr.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/sys_proto.h>

#define QB_STATE_LOAD_SIZE    SZ_64K

#define MMC_DEV		0
#define QSPI_DEV	1
#define NAND_DEV	2
#define QSPI_NOR_DEV	3
#define ROM_API_DEV	4
#define RAM_DEV		5

#if CONFIG_IS_ENABLED(DM_MMC) && CONFIG_IS_ENABLED(MMC_WRITE)
#define QB_MMC_EN	1
#endif /* DM_MMC && MMC_WRITE */
#if CONFIG_IS_ENABLED(SPI)
#define QB_SPI_EN	1
#endif /* SPI */

#define IMG_FLAGS_IMG_TYPE_MASK   0xFU
#define IMG_FLAGS_IMG_TYPE(x)     FIELD_GET(IMG_FLAGS_IMG_TYPE_MASK, x)

#define IMG_TYPE_DDR_TDATA_DUMMY  0xDU   /* dummy DDR training data image */

/**
 * Table used to implement half-byte CRC
 * Polynomial: 0xEDB88320
 */
static const u32 p_table[] = {
	0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
	0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
	0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
};

/**
 * Implement half-byte CRC algorithm
 */
static u32 qb_crc32(const void *addr, u32 len)
{
	u32 crc = ~0x00, idx, i, val;
	const u8 *chr = (const u8 *)addr;

	for (i = 0; i < len; i++, chr++) {
		val = (u32)(*chr);

		idx = (crc ^ (val >> 0)) & 0x0F;
		crc = p_table[idx] ^ (crc >> 4);
		idx = (crc ^ (val >> 4)) & 0x0F;
		crc = p_table[idx] ^ (crc >> 4);
	}

	return ~crc;
}

bool qb_check(void)
{
	struct ddrphy_qb_state *qb_state;
	u32 size, crc;

	/**
	 * Ensure CRC is not empty, the reason is that
	 * the data is invalidated after first save run
	 * or after it is overwritten.
	 */
	qb_state = (struct ddrphy_qb_state *)CONFIG_SAVED_QB_STATE_BASE;
	size = sizeof(struct ddrphy_qb_state) - sizeof(qb_state->crc);
	crc = qb_crc32(qb_state->mac, size);

	if (!qb_state->crc || crc != qb_state->crc)
		return false;

	return true;
}

#if IS_ENABLED(CONFIG_SCMI_FIRMWARE)
static int scmi_get_boot_device_offset(unsigned long *img_off)
{
	int ret;
	rom_passover_t rom_data = {0};
	rom_passover_t *rdata = &rom_data;

	ret = scmi_get_rom_data(rdata);
	if (ret != 0) {
		printf("SCMI: fail to get ROM passover data %d\n", ret);
		return ret;
	}

	*img_off = rom_data.img_ofs;

	return 0;
}

#ifdef QB_MMC_EN
static int scmi_get_boot_stage(u8 *stage)
{
	int ret;
	rom_passover_t rom_data = {0};
	rom_passover_t *rdata = &rom_data;

	ret = scmi_get_rom_data(rdata);
	if (ret != 0) {
		printf("SCMI: fail to get ROM passover data %d\n", ret);
		return ret;
	}

	*stage = rom_data.boot_stage;

	return 0;
}
#endif /** QB_MMC_EN */
#endif /** CONFIG_SCMI_FIRMWARE */

static unsigned long get_boot_device_offset(void *dev, int dev_type, bool bootdev)
{
	unsigned long offset = 0;
	struct mmc *mmc;

#if IS_ENABLED(CONFIG_SCMI_FIRMWARE)
	int ret;

	/** Running qb save from SDP boot or qb save to non boot device.
	  * The current boot device is different that the medium we
	  * are trying to save the qb data to. ROM does not know what our
	  * final boot device will be.
	  */
	if (bootdev) {
		ret = scmi_get_boot_device_offset(&offset);
		if (!ret)
			return offset;
	}
	/* fall back to boot from primary set if get rom passover failed */
#endif /** CONFIG_SCMI_FIRMWARE */

	switch (dev_type) {
	case ROM_API_DEV:
		offset = (unsigned long)dev;
		break;
	case MMC_DEV:
		mmc = (struct mmc *)dev;

		if (IS_SD(mmc) || mmc->part_config == MMCPART_NOAVAILABLE) {
			offset = CONTAINER_HDR_MMCSD_OFFSET;
		} else {
			u8 part = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);

			if (part == EMMC_BOOT_PART_BOOT1 || part == EMMC_BOOT_PART_BOOT2)
				offset = CONTAINER_HDR_EMMC_OFFSET;
			else
				offset = CONTAINER_HDR_MMCSD_OFFSET;
		}
		break;
	case QSPI_DEV:
		offset = CONTAINER_HDR_QSPI_OFFSET;
		break;
	case NAND_DEV:
		offset = CONTAINER_HDR_NAND_OFFSET;
		break;
	case QSPI_NOR_DEV:
		offset = CONTAINER_HDR_QSPI_OFFSET + 0x08000000;
		break;
	case RAM_DEV:
		offset = (unsigned long)dev + CONTAINER_HDR_MMCSD_OFFSET;
		break;
	}

	return offset;
}

static int parse_container(void *addr, u32 *qb_data_off)
{
	struct container_hdr *phdr;
	struct boot_img_t *img_entry;
	u8 i = 0;
	u32 img_type, img_end;

	phdr = (struct container_hdr *)addr;
	if (phdr->tag != 0x87 || (phdr->version != 0x0 && phdr->version != 0x2))
		return -1;

	img_entry = (struct boot_img_t *)(addr + sizeof(struct container_hdr));
	for (i = 0; i < phdr->num_images; i++) {
		img_type = IMG_FLAGS_IMG_TYPE(img_entry->hab_flags);
		if (img_type == IMG_TYPE_DDR_TDATA_DUMMY && img_entry->size == 0) {
			/* Image entry pointing to DDR Training Data */
			(*qb_data_off) = img_entry->offset;
			return 0;
		}

		img_end = img_entry->offset + img_entry->size;
		if (i + 1 < phdr->num_images) {
			img_entry++;
			if (img_end + QB_STATE_LOAD_SIZE == img_entry->offset) {
				/* hole detected */
				(*qb_data_off) = img_end;
				return 0;
			}
		}
	}

	return -1;
}

static int get_dev_qbdata_offset(void *dev, int dev_type, unsigned long offset, u32 *qbdata_offset)
{
	int ret = 0;
	u16 ctnr_hdr_align = CONTAINER_HDR_ALIGNMENT;
	void *buf = kmalloc(ctnr_hdr_align, GFP_KERNEL);

	if (!buf) {
		printf("kmalloc buffer failed\n");
		return -ENOMEM;
	}

	switch (dev_type) {
#ifdef QB_MMC_EN
	case MMC_DEV:
		unsigned long count = 0;
		struct mmc *mmc = (struct mmc *)dev;

		count = blk_dread(mmc_get_blk_desc(mmc),
				  offset / mmc->read_bl_len,
				  ctnr_hdr_align / mmc->read_bl_len,
				  buf);
		if (count == 0) {
			printf("Read container image from MMC/SD failed\n");
			free(buf);
			return -EIO;
		}
		break;
#endif /* QB_MMC_EN */
#ifdef QB_SPI_EN
	case QSPI_DEV:
		struct spi_flash *flash = (struct spi_flash *)dev;

		ret = spi_flash_read(flash, offset,
				     ctnr_hdr_align, buf);
		if (ret) {
			printf("Read container header from QSPI failed\n");
			free(buf);
			return -EIO;
		}
		break;
#endif /* QB_SPI_EN */
	case QSPI_NOR_DEV:
	case RAM_DEV:
		memcpy(buf, (const void *)offset, ctnr_hdr_align);
		break;
	default:
		printf("Support for device %d not enabled\n", dev_type);
		free(buf);
		return -EIO;
	}

	ret = parse_container(buf, qbdata_offset);

	free(buf);

	return ret;
}

static int get_qbdata_offset(void *dev, int dev_type, u32 *qbdata_offset, bool bootdev)
{
	u32 offset = get_boot_device_offset(dev, dev_type, bootdev);
	u16 ctnr_hdr_align = CONTAINER_HDR_ALIGNMENT;
	u32 cont_offset;
	int ret, i;

	for (i = 0; i < 3; i++) {
		cont_offset = offset + i * ctnr_hdr_align;
		ret = get_dev_qbdata_offset(dev, dev_type, cont_offset, qbdata_offset);
		if (ret == 0) {
			(*qbdata_offset) += cont_offset;
			break;
		}
	}

	return ret;
}

#ifdef QB_MMC_EN
static int mmc_get_device_index(u32 dev)
{
	switch (dev) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2:
		return 1;
	}

	return -ENODEV;
}

static int mmc_find_device(struct mmc **mmcp, int mmc_dev)
{
	int err;

	err = mmc_init_device(mmc_dev);
	if (err)
		return err;

	*mmcp = find_mmc_device(mmc_dev);

	return (*mmcp) ? 0 : -ENODEV;
}

static int do_qb_mmc(int dev, bool save, bool is_bootdev)
{
	struct mmc *mmc;
	int ret = 0, mmc_dev;
	bool has_hw_part;
	u8 orig_part, part;
	u32 offset;
	void *buf;

	mmc_dev = mmc_get_device_index(dev);
	if (mmc_dev < 0)
		return mmc_dev;

	ret = mmc_find_device(&mmc, mmc_dev);
	if (ret)
		return ret;

	if (!mmc->has_init)
		ret = mmc_init(mmc);

	if (ret)
		return ret;

	has_hw_part = !IS_SD(mmc) && mmc->part_config != MMCPART_NOAVAILABLE;

	if (has_hw_part) {
		orig_part = mmc_get_blk_desc(mmc)->hwpart;
		part = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);

		if (is_bootdev && (part == 1 || part == 2)) {
#if IS_ENABLED(CONFIG_SCMI_FIRMWARE)
			u8 stage;
			ret = scmi_get_boot_stage(&stage);
			if (!ret) {
				if (stage == 0x9)
					part = (part == 1) ? 2 : 1;
			}
#endif /** CONFIG_SCMI_FIRMWARE */
		}

		/** Select the partition */
		ret = mmc_switch_part(mmc, part);
		if (ret)
			return ret;
	}

	ret = get_qbdata_offset(mmc, MMC_DEV, &offset, is_bootdev);
	if (ret)
		return ret;

	if (save) {
		/* QB data is stored in DDR -> can use it as buf */
		buf = (void *)CONFIG_SAVED_QB_STATE_BASE;
		ret = blk_dwrite(mmc_get_blk_desc(mmc),
				 offset / mmc->write_bl_len,
				 QB_STATE_LOAD_SIZE / mmc->write_bl_len,
				 (const void *)buf);
	} else {
		/* erase */
		ret = blk_derase(mmc_get_blk_desc(mmc),
				 offset / mmc->write_bl_len,
				 QB_STATE_LOAD_SIZE / mmc->write_bl_len);
	}

	ret = (ret > 0) ? 0 : -1;

	/* Return to original partition */
	if (has_hw_part)
		ret |= mmc_switch_part(mmc, orig_part);

	return ret;
}
#else
static int do_qb_mmc(int dev, bool save, bool is_bootdev)
{
	printf("Please enable MMC and MMC_WRITE\n");

	return -EOPNOTSUPP;
}
#endif /* QB_MMC_EN */

#ifdef QB_SPI_EN
static int spi_find_device(struct spi_flash **dev)
{
	unsigned int sf_bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int sf_cs = CONFIG_SF_DEFAULT_CS;
	struct spi_flash *flash;
	int ret = 0;

	flash = spi_flash_probe(sf_bus, sf_cs,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);

	if (!flash) {
		puts("SPI probe failed.\n");
		return -ENODEV;
	}

	*dev = flash;

	return ret;
}

static int do_qb_spi(int dev, bool save, bool is_bootdev)
{
	int ret = 0;
	u32 offset;
	void *buf;
	struct spi_flash *flash;

	ret = spi_find_device(&flash);
	if (ret)
		return -ENODEV;

	ret = get_qbdata_offset(flash, QSPI_DEV, &offset, is_bootdev);
	if (ret)
		return ret;

	ret = spi_flash_erase(flash, offset,
			      QB_STATE_LOAD_SIZE);

	if (!ret && save) {
		/* QB data is stored in DDR -> can use it as buf */
		buf = (void *)CONFIG_SAVED_QB_STATE_BASE;
		ret = spi_flash_write(flash, offset,
				      QB_STATE_LOAD_SIZE, buf);
	}

	return ret;
}
#else
static int do_qb_spi(int dev, bool save, bool is_bootdev)
{
	printf("Please enable SPI\n");

	return -EOPNOTSUPP;
}
#endif /* QB_SPI_EN */

int qb(int qb_dev, int qb_bootdev, bool save)
{
	int ret = -1;

	if (save && !qb_check())
		return ret;

	switch (qb_dev) {
	case BOOT_DEVICE_MMC1:
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2:
		ret = do_qb_mmc(qb_dev, save, !!(qb_dev == qb_bootdev));
		break;
	case BOOT_DEVICE_SPI:
		ret = do_qb_spi(qb_dev, save, !!(qb_dev == qb_bootdev));
		break;
	default:
		printf("Unsupported quickboot device\n");
		break;
	}

	if (ret)
		return ret;

	if (!save)
		return 0;

	/**
	 * invalidate qb_state mem so that at next boot
	 * the check function will fail and save won't happen
	 */
	memset((void *)CONFIG_SAVED_QB_STATE_BASE, 0, sizeof(struct ddrphy_qb_state));

	return 0;
}

void spl_qb_save(void)
{
	int dev = spl_boot_device();

	/* Save QB data on current boot device */
	if (qb(dev, dev, true))
		printf("QB save failed\n");
}
