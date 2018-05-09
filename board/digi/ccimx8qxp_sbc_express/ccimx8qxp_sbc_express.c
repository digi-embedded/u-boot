/*
 * Copyright (C) 2018 Digi International, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <netdev.h>
#include <fsl_ifc.h>
#include <fdt_support.h>
#include <libfdt.h>
#include <environment.h>
#include <fsl_esdhc.h>
#include <i2c.h>
#include "pca953x.h"
#include <otf_update.h>

#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/clock.h>
#include <asm/imx-common/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <dm.h>
#include <imx8_hsio.h>
#include <usb.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sys_proto.h>
#include <asm/imx-common/boot_mode.h>
#include <asm/imx-common/video.h>
#include <asm/arch/video_common.h>
#include <power-domain.h>
#include "../../freescale/common/tcpc.h"

DECLARE_GLOBAL_DATA_PTR;

static struct blk_desc *mmc_dev;
static int mmc_dev_index = -1;

#define ESDHC_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ESDHC_CLK_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))


#define ENET_INPUT_PAD_CTRL	((SC_PAD_CONFIG_OD_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ENET_NORMAL_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define FSPI_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define GPIO_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define I2C_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define UART_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static iomux_cfg_t uart2_pads[] = {
	SC_P_UART2_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	SC_P_UART2_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static void setup_iomux_uart(void)
{
	imx8_iomux_setup_multiple_pads(uart2_pads, ARRAY_SIZE(uart2_pads));
}

int board_early_init_f(void)
{
	sc_ipc_t ipcHndl = 0;
	sc_err_t sciErr = 0;

	ipcHndl = gd->arch.ipc_channel_handle;

	/* Power up UART2 */
	sciErr = sc_pm_set_resource_power_mode(ipcHndl, SC_R_UART_2, SC_PM_PW_MODE_ON);
	if (sciErr != SC_ERR_NONE)
		return 0;

	/* Set UART2 clock root to 80 MHz */
	sc_pm_clock_rate_t rate = 80000000;
	sciErr = sc_pm_set_clock_rate(ipcHndl, SC_R_UART_2, 2, &rate);
	if (sciErr != SC_ERR_NONE)
		return 0;

	/* Enable UART2 clock root */
	sciErr = sc_pm_clock_enable(ipcHndl, SC_R_UART_2, 2, true, false);
	if (sciErr != SC_ERR_NONE)
		return 0;

	setup_iomux_uart();

	return 0;
}

#ifdef CONFIG_FSL_ESDHC

struct gpio_desc usdhc2_cd;

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

int board_has_emmc(void)
{
	return 1;
}

int board_mmc_init(bd_t *bis)
{
	int ret = 0;

	/* Request uSDHC2 card detect GPIO */
	ret = dm_gpio_lookup_name("gpio5_9", &usdhc2_cd);
	if (ret)
		return ret;

	ret = dm_gpio_request(&usdhc2_cd, "usdhc2_cd");
	if (ret)
		return ret;

	return 0;
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC1_BASE_ADDR:
		ret = 1; /* eMMC */
		break;
	case USDHC2_BASE_ADDR:
		ret = dm_gpio_get_value(&usdhc2_cd);
		break;
	}

	return ret;
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

#ifdef CONFIG_FEC_MXC
#include <miiphy.h>

static void enet_device_phy_reset(void)
{
	struct gpio_desc desc;
	struct udevice *dev = NULL;
	int ret;

	if (0 == CONFIG_FEC_ENET_DEV) {
		ret = dm_gpio_lookup_name("gpio5_1", &desc);
		if (ret)
			return;

		ret = dm_gpio_request(&desc, "enet0_reset");
		if (ret)
			return;

		dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
		dm_gpio_set_value(&desc, 0);
		udelay(100);
		dm_gpio_set_value(&desc, 1);
		dm_gpio_free(dev, &desc);
	}
}

int board_phy_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x8190);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static int setup_fec(int ind)
{
	struct gpio_desc enet_pwr;
	int ret;

	/* Power up the PHY */
	ret = dm_gpio_lookup_name("gpio5_8", &enet_pwr);
	if (ret)
		return -1;

	ret = dm_gpio_request(&enet_pwr, "enet0_pwr");
	if (ret)
		return -1;

	dm_gpio_set_dir_flags(&enet_pwr, GPIOD_IS_OUT);
	dm_gpio_set_value(&enet_pwr, 1);
	mdelay(26);	/* PHY power up time */

	/* Reset ENET PHY */
	enet_device_phy_reset();

	return 0;
}
#endif

#ifdef CONFIG_MXC_GPIO
static iomux_cfg_t board_gpios[] = {
};

static void board_gpio_init(void)
{
	imx8_iomux_setup_multiple_pads(board_gpios, ARRAY_SIZE(board_gpios));
}
#endif

int checkboard(void)
{
	puts("Board: iMX8QXP MEK\n");

	print_bootinfo();

	/* Note:  After reloc, ipcHndl will no longer be valid.  If handle
	 *        returned by sc_ipc_open matches SC_IPC_CH, use this
	 *        macro (valid after reloc) for subsequent SCI calls.
	 */
	if (gd->arch.ipc_channel_handle != SC_IPC_CH)
		printf("\nSCI error! Invalid handle\n");

#ifdef SCI_FORCE_ABORT
	sc_rpc_msg_t abort_msg;

	puts("Send abort request\n");
	RPC_SIZE(&abort_msg) = 1;
	RPC_SVC(&abort_msg) = SC_RPC_SVC_ABORT;
	sc_ipc_write(SC_IPC_CH, &abort_msg);

	/* Close IPC channel */
	sc_ipc_close(SC_IPC_CH);
#endif /* SCI_FORCE_ABORT */

	return 0;
}

#ifdef CONFIG_FSL_HSIO

#define PCIE_PAD_CTRL	((SC_PAD_CONFIG_OD_IN << PADRING_CONFIG_SHIFT))
static iomux_cfg_t board_pcie_pins[] = {
	SC_P_PCIE_CTRL0_CLKREQ_B | MUX_MODE_ALT(0) | MUX_PAD_CTRL(PCIE_PAD_CTRL),
	SC_P_PCIE_CTRL0_WAKE_B | MUX_MODE_ALT(0) | MUX_PAD_CTRL(PCIE_PAD_CTRL),
	SC_P_PCIE_CTRL0_PERST_B | MUX_MODE_ALT(0) | MUX_PAD_CTRL(PCIE_PAD_CTRL),
};

static void imx8qxp_hsio_initialize(void)
{
	struct power_domain pd;
	int ret;

	if (!power_domain_lookup_name("hsio_pcie1", &pd)) {
		ret = power_domain_on(&pd);
		if (ret)
			printf("hsio_pcie1 Power up failed! (error = %d)\n", ret);
	}

	imx8_iomux_setup_multiple_pads(board_pcie_pins, ARRAY_SIZE(board_pcie_pins));
}

void pci_init_board(void)
{
	imx8qxp_hsio_initialize();

	/* test the 1 lane mode of the PCIe A controller */
	mx8qxp_pcie_init();
}

#endif

#ifdef CONFIG_USB_XHCI_IMX8

#define USB_TYPEC_SEL IMX_GPIO_NR(5, 9)
static iomux_cfg_t ss_mux_gpio[] = {
	SC_P_ENET0_REFCLK_125M_25M | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
};

struct udevice *tcpc_i2c_dev = NULL;

static void setup_typec(void)
{
	struct udevice *bus;
	uint8_t chip = 0x50;
	int ret;
	struct gpio_desc typec_en_desc;

	imx8_iomux_setup_multiple_pads(ss_mux_gpio, ARRAY_SIZE(ss_mux_gpio));
	gpio_request(USB_TYPEC_SEL, "typec_sel");

	ret = dm_gpio_lookup_name("gpio@1a_7", &typec_en_desc);
	if (ret) {
		printf("%s lookup gpio@1a_7 failed ret = %d\n", __func__, ret);
		return;
	}

	ret = dm_gpio_request(&typec_en_desc, "typec_en");
	if (ret) {
		printf("%s request typec_en failed ret = %d\n", __func__, ret);
		return;
	}

	/* Enable SS MUX */
	dm_gpio_set_dir_flags(&typec_en_desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	ret = uclass_get_device_by_seq(UCLASS_I2C, 1, &bus);
	if (ret) {
		printf("%s: Can't find bus\n", __func__);
		return;
	}

	ret = dm_i2c_probe(bus, chip, 0, &tcpc_i2c_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x\n",
			__func__, chip);
		return;
	}

	tcpc_init(tcpc_i2c_dev);
}

void ss_mux_select(enum typec_cc_polarity pol)
{
	if (pol == TYPEC_POLARITY_CC1)
		gpio_direction_output(USB_TYPEC_SEL, 0);
	else
		gpio_direction_output(USB_TYPEC_SEL, 1);
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (init == USB_INIT_HOST && tcpc_i2c_dev)
		ret = tcpc_setup_dfp_mode(tcpc_i2c_dev, &ss_mux_select);

	return ret;

}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (init == USB_INIT_HOST && tcpc_i2c_dev)
		ret = tcpc_disable_vbus(tcpc_i2c_dev);

	return ret;
}
#endif

#ifdef CONFIG_RESET_PHY_R
void reset_phy(void)
{
	enet_device_phy_reset();
}
#endif /* CONFIG_RESET_PHY_R */


int board_init(void)
{
#ifdef CONFIG_MXC_GPIO
	board_gpio_init();
#endif

	if (board_mmc_init(NULL))
		printf("  Error initializing MMC\n");

#ifdef CONFIG_FEC_MXC
	setup_fec(CONFIG_FEC_ENET_DEV);
#endif

#ifdef CONFIG_USB_XHCI_IMX8
	setup_typec();
#endif

	return 0;
}

void board_quiesce_devices()
{
	const char *power_on_devices[] = {
		"dma_lpuart2",

		/* HIFI DSP boot */
		"audio_sai0",
		"audio_ocram",
	};

	power_off_pd_devices(power_on_devices, ARRAY_SIZE(power_on_devices));
}

void detail_board_ddr_info(void)
{
	puts("\nDDR    ");
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

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	return 0;
}
#endif

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	setenv("board_name", "MEK");
	setenv("board_rev", "iMX8QXP");
#endif

	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/

#if defined(CONFIG_VIDEO_IMXDPUV1)
static void enable_lvds(struct display_info_t const *dev)
{
	struct gpio_desc desc;
	int ret;

	/* MIPI_DSI0_EN on IOEXP 0x1a port 6, MIPI_DSI1_EN on IOEXP 0x1d port 7 */
	ret = dm_gpio_lookup_name("gpio@1a_6", &desc);
	if (ret)
		return;

	ret = dm_gpio_request(&desc, "lvds0_en");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

	display_controller_setup((PS2KHZ(dev->mode.pixclock) * 1000));
	lvds_soc_setup(dev->bus, (PS2KHZ(dev->mode.pixclock) * 1000));
	lvds_configure(dev->bus);
	lvds2hdmi_setup(13);
}

struct display_info_t const displays[] = {{
	.bus	= 0, /* LVDS0 */
	.addr	= 0, /* LVDS0 */
	.pixfmt	= IMXDPUV1_PIX_FMT_BGRA32,
	.detect	= NULL,
	.enable	= enable_lvds,
	.mode	= {
		.name           = "IT6263", /* 720P60 */
		.refresh        = 60,
		.xres           = 1280,
		.yres           = 720,
		.pixclock       = 13468, /* 74250000 */
		.left_margin    = 110,
		.right_margin   = 220,
		.upper_margin   = 5,
		.lower_margin   = 20,
		.hsync_len      = 40,
		.vsync_len      = 5,
		.sync           = FB_SYNC_EXT,
		.vmode          = FB_VMODE_NONINTERLACED
} } };
size_t display_count = ARRAY_SIZE(displays);

#endif /* CONFIG_VIDEO_IMXDPUV1 */
