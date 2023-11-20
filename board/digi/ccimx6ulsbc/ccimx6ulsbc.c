/*
 * Copyright (C) 2016-2018 Digi International, Inc
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <init.h>
#include <net.h>
#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6ul_pins.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/mach-imx/video.h>
#include <asm/io.h>
#include <bmp_layout.h>
#include <common.h>
#include <env.h>
#include <fsl_esdhc_imx.h>
#include <i2c.h>
#include <linux/delay.h>
#include <linux/sizes.h>
#include <linux/fb.h>
#include <malloc.h>
#include <miiphy.h>
#include <mmc.h>
#include <netdev.h>
#include <usb.h>
#include <usb/ehci-ci.h>
#include <command.h>
#include "../ccimx6ul/ccimx6ul.h"
#include "../common/carrier_board.h"
#include "../common/helper.h"
#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "../common/trustfence.h"

#ifdef CONFIG_POWER
#include <power/pmic.h>
#include <power/pfuze3000_pmic.h>
#include "../../freescale/common/pfuze.h"
#endif

#ifdef CONFIG_FSL_FASTBOOT
#include <fastboot.h>
#ifdef CONFIG_ANDROID_RECOVERY
#include <recovery.h>
#endif
#endif /*CONFIG_FSL_FASTBOOT*/

DECLARE_GLOBAL_DATA_PTR;

unsigned int board_version = CARRIERBOARD_VERSION_UNDEFINED;
unsigned int board_id = CARRIERBOARD_ID_UNDEFINED;

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_22K_UP  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_SPEED_HIGH   |                                   \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST)

#define MDIO_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST | PAD_CTL_ODE)

#define ENET_CLK_PAD_CTRL  (PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

#define ENET_RX_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |          \
	PAD_CTL_SPEED_HIGH   | PAD_CTL_SRE_FAST)

#define OTG_ID_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define GPI_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |            \
	PAD_CTL_PUS_100K_DOWN | PAD_CTL_SPEED_MED |               \
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

#define LCD_PAD_CTRL    (PAD_CTL_HYS | PAD_CTL_PUS_100K_UP | PAD_CTL_PUE | \
	PAD_CTL_PKE | PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm)

static iomux_v3_cfg_t const uart5_pads[] = {
	MX6_PAD_UART5_TX_DATA__UART5_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_UART5_RX_DATA__UART5_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
static iomux_v3_cfg_t const ext_gpios_pads[] = {
	MX6_PAD_GPIO1_IO05__GPIO1_IO05 | MUX_PAD_CTRL(GPI_PAD_CTRL),
	MX6_PAD_GPIO1_IO03__GPIO1_IO03 | MUX_PAD_CTRL(GPI_PAD_CTRL),
	MX6_PAD_GPIO1_IO02__GPIO1_IO02 | MUX_PAD_CTRL(GPI_PAD_CTRL),
};

static void setup_iomux_ext_gpios(void)
{
	imx_iomux_v3_setup_multiple_pads(ext_gpios_pads,
					 ARRAY_SIZE(ext_gpios_pads));
}
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */

/* micro SD */
static iomux_v3_cfg_t const usdhc2_pads[] = {
	MX6_PAD_CSI_VSYNC__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_CSI_HSYNC__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_CSI_DATA00__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_CSI_DATA01__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_CSI_DATA02__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_CSI_DATA03__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

#ifdef CONFIG_FEC_MXC
/*
 * pin conflicts for fec1 and fec2, GPIO1_IO06 and GPIO1_IO07 can only
 * be used for ENET1 or ENET2, cannot be used for both.
 */
static iomux_v3_cfg_t const fec1_pads[] = {
	MX6_PAD_GPIO1_IO06__ENET1_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),
	MX6_PAD_GPIO1_IO07__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_DATA0__ENET1_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_DATA1__ENET1_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_EN__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
	MX6_PAD_ENET1_RX_DATA0__ENET1_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_DATA1__ENET1_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_ER__ENET1_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_EN__ENET1_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static iomux_v3_cfg_t const fec2_pads[] = {
	MX6_PAD_GPIO1_IO06__ENET2_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),
	MX6_PAD_GPIO1_IO07__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX6_PAD_ENET2_TX_DATA0__ENET2_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_TX_DATA1__ENET2_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
	MX6_PAD_ENET2_TX_EN__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX6_PAD_ENET2_RX_DATA0__ENET2_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_DATA1__ENET2_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_EN__ENET2_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_ER__ENET2_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static void setup_iomux_fec(int fec_id)
{
	if (fec_id == 0)
		imx_iomux_v3_setup_multiple_pads(fec1_pads, ARRAY_SIZE(fec1_pads));
	else
		imx_iomux_v3_setup_multiple_pads(fec2_pads, ARRAY_SIZE(fec2_pads));
}
#endif

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart5_pads, ARRAY_SIZE(uart5_pads));
}

#ifdef CONFIG_FSL_ESDHC_IMX
static struct fsl_esdhc_cfg usdhc_cfg[] = {
	{USDHC2_BASE_ADDR, 0, 4},
};

int board_mmc_getcd(struct mmc *mmc)
{
	/* CD not connected. Assume microSD card present */
	return 1;
}

int board_mmc_init(struct bd_info *bis)
{
	int i, ret;

	/*
	 * According to the board_mmc_init() the following map is done:
	 * (U-boot device node)    (Physical Port)
	 * mmc0                    USDHC2
	 */
	for (i = 0; i < CFG_SYS_FSL_USDHC_NUM; i++) {
		switch (i) {
		case 0:
			imx_iomux_v3_setup_multiple_pads(
				usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
				"(%d) than supported by the board\n", i + 1);
			return -EINVAL;
		}

		ret = fsl_esdhc_initialize(bis, &usdhc_cfg[i]);
		if (ret) {
			printf("Warning: failed to initialize mmc dev %d\n", i);
		}
	}

	return 0;
}
#endif /* CONFIG_FSL_ESDHC_IMX */

#ifdef CONFIG_FEC_MXC
void reset_phy(void)
{
	int reset;

	/*
	 * The reset line must be held low for a minimum of 100usec and cannot
	 * be deasserted before 25ms have passed since the power supply has
	 * reached 80% of the operating voltage. At this point of the code
	 * we can assume the second premise is already accomplished.
	 */
	if (CONFIG_FEC_ENET_DEV == 0) {
		/* MCA_IO7 is connected to PHY reset */
		reset = (1 << 7);
		/* Configure as output */
		mca_update_bits(MCA_GPIO_DIR_0, reset, reset);
		/* Assert PHY reset (low) */
		mca_update_bits(MCA_GPIO_DATA_0, reset, 0);
		udelay(100);
		/* Deassert PHY reset (high) */
		mca_update_bits(MCA_GPIO_DATA_0, reset, reset);
	} else if (CONFIG_FEC_ENET_DEV == 1) {
		/* CPU GPIO5_6 is connected to PHY reset */
		reset = IMX_GPIO_NR(5, 6);
		/* Assert PHY reset (low) */
		gpio_request(reset, "ENET PHY Reset");
		gpio_direction_output(reset, 0);
		udelay(100);
		/* Deassert PHY reset (high) */
		gpio_set_value(reset, 1);
	}
}

int board_eth_init(struct bd_info *bis)
{
	int ret;

	setup_iomux_fec(CONFIG_FEC_ENET_DEV);

	ret = fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV,
		CONFIG_FEC_MXC_PHYADDR, IMX_FEC_BASE);
	if (ret)
		printf("FEC%d MXC: %s:failed\n", CONFIG_FEC_ENET_DEV, __func__);

	reset_phy();

	return 0;
}

static int setup_fec(int fec_id)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs
		= (struct iomuxc_gpr_base_regs *) IOMUXC_GPR_BASE_ADDR;
	int ret;

	if (0 == fec_id) {
		if (check_module_fused(MODULE_ENET1))
			return -1;

		/* Use 50M anatop loopback REF_CLK1 for ENET1, clear gpr1[13], set gpr1[17]*/
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1], IOMUX_GPR1_FEC1_MASK,
				IOMUX_GPR1_FEC1_CLOCK_MUX1_SEL_MASK);
	} else {
		if (check_module_fused(MODULE_ENET2))
			return -1;

		/* Use 50M anatop loopback REF_CLK2 for ENET2, clear gpr1[14], set gpr1[18]*/
		clrsetbits_le32(&iomuxc_gpr_regs->gpr[1], IOMUX_GPR1_FEC2_MASK,
				IOMUX_GPR1_FEC2_CLOCK_MUX1_SEL_MASK);
	}

	ret = enable_fec_anatop_clock(fec_id, ENET_50MHZ);
	if (ret)
		return ret;

	enable_enet_clk(1);

	return 0;
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)

static iomux_v3_cfg_t const usb_otg_pads[] = {
	MX6_PAD_GPIO1_IO00__ANATOP_OTG1_ID | MUX_PAD_CTRL(OTG_ID_PAD_CTRL),
};

/* At default the 3v3 enables the MIC2026 for VBUS power */
static void setup_usb(void)
{
	imx_iomux_v3_setup_multiple_pads(usb_otg_pads,
					 ARRAY_SIZE(usb_otg_pads));
}

int board_usb_phy_mode(int port)
{
	if (port == 1)
		return USB_INIT_HOST;
	else
		return usb_phy_mode(port);
}

int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 1)
		return -EINVAL;

	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);

	/* Set Power polarity */
	setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);

	return 0;
}
#endif

#ifdef CONFIG_VIDEO_MXS

/* GPIO used to control the display backlight */

static iomux_v3_cfg_t const lcd_pads[] = {
	MX6_PAD_LCD_CLK__LCDIF_CLK | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_ENABLE__LCDIF_ENABLE | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_HSYNC__LCDIF_HSYNC | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_VSYNC__LCDIF_VSYNC | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA00__LCDIF_DATA00 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA01__LCDIF_DATA01 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA02__LCDIF_DATA02 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA03__LCDIF_DATA03 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA04__LCDIF_DATA04 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA05__LCDIF_DATA05 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA06__LCDIF_DATA06 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA07__LCDIF_DATA07 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA08__LCDIF_DATA08 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA09__LCDIF_DATA09 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA10__LCDIF_DATA10 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA11__LCDIF_DATA11 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA12__LCDIF_DATA12 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA13__LCDIF_DATA13 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA14__LCDIF_DATA14 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA15__LCDIF_DATA15 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA16__LCDIF_DATA16 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA17__LCDIF_DATA17 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA18__LCDIF_DATA18 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA19__LCDIF_DATA19 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA20__LCDIF_DATA20 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA21__LCDIF_DATA21 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA22__LCDIF_DATA22 | MUX_PAD_CTRL(LCD_PAD_CTRL),
	MX6_PAD_LCD_DATA23__LCDIF_DATA23 | MUX_PAD_CTRL(LCD_PAD_CTRL),

	/* Use GPIO for Brightness adjustment */
	MX6_PAD_NAND_DQS__GPIO4_IO16 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

void board_video_bl_enable(bool enable)
{
	int polarity = BACKLIGHT_ENABLE_POLARITY;

	/* Control the display backlight through the corresponding gpio */
	gpio_direction_output(BACKLIGHT_GPIO, enable ? polarity : !polarity);
}

void do_enable_parallel_lcd(struct display_info_t const *dev)
{
	enable_lcdif_clock(dev->bus, 1);

	imx_iomux_v3_setup_multiple_pads(lcd_pads, ARRAY_SIZE(lcd_pads));

	/* Disable the bl until everything is setup */
	gpio_request(BACKLIGHT_GPIO, "backlight");
	board_video_bl_enable(false);
}

struct display_info_t const displays[] = {
	{
		.bus = MX6UL_LCDIF1_BASE_ADDR,
		.addr = 0,
		.pixfmt = 18,
		.detect = NULL,
		.enable	= do_enable_parallel_lcd,
		.mode	= {
			.name		= "G101EVN010",
			.xres           = 1280,
			.yres           = 800,
			.pixclock       = 14507,
			.left_margin    = 0,
			.right_margin   = 120,
			.upper_margin   = 10,
			.lower_margin   = 0,
			.hsync_len      = 8,
			.vsync_len      = 6,
			.sync           = FB_SYNC_CLK_LAT_FALL | \
					  FB_SYNC_HOR_HIGH_ACT | \
					  FB_SYNC_VERT_HIGH_ACT,
			.vmode          = FB_VMODE_NONINTERLACED
		}
	}, {
		.bus = MX6UL_LCDIF1_BASE_ADDR,
		.addr = 0,
		.pixfmt = 18,
		.detect = NULL,
		.enable	= do_enable_parallel_lcd,
		.mode	= {
			.name		= "F07A0102",
			.xres           = 800,
			.yres           = 480,
			.pixclock       = 30066,
			.left_margin    = 0,
			.right_margin   = 50,
			.upper_margin   = 25,
			.lower_margin   = 10,
			.hsync_len      = 128,
			.vsync_len      = 10,
			.sync           = FB_SYNC_CLK_LAT_FALL,
			.vmode          = FB_VMODE_NONINTERLACED
		}
	},
};

size_t display_count = ARRAY_SIZE(displays);

/* We do not want any output on the display, just the splash or logo */
int board_cfb_skip(void)
{
	return 1;
}

static int check_bmp(ulong addr, u32 *width, u32 *height)
{
	struct bmp_image *bmp = (struct bmp_image *)addr;
	void *bmp_alloc_addr = NULL;
	unsigned long len;

	if (!((bmp->header.signature[0]=='B') &&
	      (bmp->header.signature[1]=='M')))
		/* If there is no signature, check for a compressed file */
		bmp = gunzip_bmp(addr, &len, &bmp_alloc_addr);

	if (bmp == NULL)
		return 1;

	*width = le32_to_cpu(bmp->header.width);
	*height = le32_to_cpu(bmp->header.height);

	return 0;
}

int board_load_logo(void)
{
	int ret, argc;
	struct load_fw fwinfo;
	char *source[] = {"", "linux", "nand", "", ""};
	char const *logosrc = env_get("logosrc");
	char *r = NULL;

	/*
	 * The variable "logosrc" can be used to change the location of the logo
	 * to load it, for instance, from the SD card. The syntax for that
	 * variable is just like the parameter list of the dboot command.
	 * For instance, to load the logo from a file on the first partition of
	 * the SD card called "mylogo.bmp", the contents of the variable should
	 * be: "linux mmc 0 mylogo.bmp".
	 * If the variable is not declared, the default is to load the file
	 * "logo.bmp" from the linux partition on the nand.
	 */
	if (logosrc) {
		char *tok, *end;

		argc = 1;

		r = strdup(logosrc);
		if (!r)
			return 1;

		tok = r;
		end = r;

		while (tok != NULL) {
			strsep(&end, " ");
			if (argc < ARRAY_SIZE(source))
				source[argc] = tok;
			tok = end;
			argc++;
		}
	} else {
		/* The default logo source uses 3 arguments */
		argc = 3;
	}

	memset(&fwinfo, 0, sizeof(fwinfo));
	if (get_source(argc, source, &fwinfo)) {
		ret = 1;
		goto ll_exit;
	}

	ret = get_fw_filename(argc, source, &fwinfo);
	if (ret) {
		/* Filename was not provided. Look for default one */
		strncpy(fwinfo.filename, "logo.bmp", sizeof(fwinfo.filename));
	}

	strncpy(fwinfo.loadaddr, "$loadaddr", sizeof(fwinfo.loadaddr));

	/* Load the bmp in memory */
	ret = load_firmware(&fwinfo, NULL);

ll_exit:
	free(r);
	return ret;
}

int board_display_logo(void)
{
	int ret, xpos, ypos;
	u32 logoheight, logowidth;
	unsigned long loadaddr;
	struct display_panel disp;

	/* Get the logo image from the selected media */
	ret = board_load_logo();
	if (ret != LDFW_LOADED) {
		printf("ERR: failed to load logo\n");
		return ret;
	}

	/* Check the format and get the size */
	loadaddr = env_get_ulong("loadaddr", 16, CONFIG_SYS_LOAD_ADDR);
	ret = check_bmp(loadaddr, &logowidth, &logoheight);
	if (ret) {
		printf("ERR: invalid logo bmp image\n");
		return ret;
	}

	/* Center the image in the display and show it */
	mxs_lcd_get_panel(&disp);
	xpos = (disp.width - logowidth) / 2;
	ypos = (disp.height - logoheight) / 2;

	return bmp_display(loadaddr, xpos, ypos);
}
#endif

int board_early_init_f(void)
{
	setup_iomux_uart();

#ifdef CONFIG_CONSOLE_DISABLE
	gd->flags |= (GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#endif

	return 0;
}

#ifdef CONFIG_POWER
int power_init_board(void)
{
	/* SOM power init */
	power_init_ccimx6ul();

	return 0;
}
#endif

int board_init(void)
{
	/* SOM init */
	ccimx6ul_init();

	board_version = get_carrierboard_version();
	board_id = get_carrierboard_id();

#ifdef	CONFIG_FEC_MXC
	setup_fec(CONFIG_FEC_ENET_DEV);
#endif

#ifdef CONFIG_USB_EHCI_MX6
	setup_usb();
#endif

	return 0;
}

void platform_default_environment(void)
{
	char cmd[80];

	som_default_environment();

	/* Set $board_version variable if defined in OTP bits */
	if (board_version > 0) {
		sprintf(cmd, "setenv -f board_version %d", board_version);
		run_command(cmd, 0);
	}

	/* Set $board_id variable if defined in OTP bits */
	if (board_id > 0) {
		sprintf(cmd, "setenv -f board_id %d", board_id);
		run_command(cmd, 0);
	}
}

int board_late_init(void)
{
#ifdef CONFIG_CONSOLE_ENABLE_GPIO
	const char *ext_gpios[] = {
		"GPIO1_5",	/* J30.11 */
		"GPIO1_3",	/* J30.12 */
		"GPIO1_2",	/* J30.13 */
	};
	const char *ext_gpio_name = ext_gpios[CONFIG_CONSOLE_ENABLE_GPIO_NR];

	setup_iomux_ext_gpios();

	if (console_enable_gpio(ext_gpio_name))
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#endif
	/* SOM late init */
	ccimx6ul_late_init();

	/* Set default dynamic variables */
	platform_default_environment();

	set_wdog_reset((struct wdog_regs *)WDOG1_BASE_ADDR);

#ifdef CONFIG_VIDEO_MXS
	board_display_logo();
	board_video_bl_enable(true);
#endif

	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
/* Platform function to modify the FDT as needed */
int ft_board_setup(void *blob, struct bd_info *bd)
{
	fdt_fixup_ccimx6ul(blob);
	fdt_fixup_carrierboard(blob);

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

int checkboard(void)
{
	print_ccimx6ul_info();
	print_carrierboard_info();
	printf("Boot device:  %s\n",
	       is_boot_from_usb() ? "USB" : get_boot_device_name());

	return 0;
}
