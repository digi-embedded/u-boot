/*
 * Copyright (C) 2016-2018 Digi International, Inc.
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/imx-common/boot_mode.h>
#include <asm/imx-common/mxc_i2c.h>
#include <asm/imx-common/hab.h>
#include <asm/io.h>
#include <common.h>
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif
#include <i2c.h>
#include <linux/sizes.h>
#include <nand.h>

#ifdef CONFIG_POWER
#include <power/pmic.h>
#include <power/pfuze3000_pmic.h>
#include "../../freescale/common/pfuze.h"
#endif
#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "../common/trustfence.h"
#include "../common/tamper.h"

DECLARE_GLOBAL_DATA_PTR;

extern bool bmode_reset;
struct digi_hwid my_hwid;
#ifdef CONFIG_HAS_TRUSTFENCE
extern int rng_swtest_status;
#endif

#define MCA_CC6UL_DEVICE_ID_VAL		0x61

#define MDIO_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST | PAD_CTL_ODE)

#define I2C_PAD_CTRL    (PAD_CTL_PKE | PAD_CTL_PUE |            \
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |               \
	PAD_CTL_DSE_40ohm | PAD_CTL_HYS |			\
	PAD_CTL_ODE)

#define GPMI_PAD_CTRL0 (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define GPMI_PAD_CTRL1 (PAD_CTL_DSE_40ohm | PAD_CTL_SPEED_MED | \
			PAD_CTL_SRE_FAST)
#define GPMI_PAD_CTRL2 (GPMI_PAD_CTRL0 | GPMI_PAD_CTRL1)

#ifdef CONFIG_SYS_I2C_MXC
#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)
/* I2C1 for PMIC and MCA */
struct i2c_pads_info i2c1_pad_info = {
	.scl = {
		.i2c_mode =  MX6_PAD_UART4_TX_DATA__I2C1_SCL | PC,
		.gpio_mode = MX6_PAD_UART4_TX_DATA__GPIO1_IO28 | PC,
		.gp = IMX_GPIO_NR(1, 28),
	},
	.sda = {
		.i2c_mode = MX6_PAD_UART4_RX_DATA__I2C1_SDA | PC,
		.gpio_mode = MX6_PAD_UART4_RX_DATA__GPIO1_IO29 | PC,
		.gp = IMX_GPIO_NR(1, 29),
	},
};
#endif

static struct ccimx6_variant ccimx6ul_variants[] = {
/* 0x00 */ { IMX6_NONE,	0, 0, "Unknown"},
/* 0x01 */ { IMX6_NONE,	0, 0, "Unknown"},
/* 0x02 - 55001944-01 */
	{
		IMX6UL,
		MEM_256MB,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH,
		"Industrial Ultralite 528MHz, 256MB NAND, 256MB DDR3, -40/+85C, Wireless, Bluetooth",
	},
/* 0x03 - 55001944-02 */
	{
		IMX6UL,
		MEM_256MB,
		0,
		"Industrial Ultralite 528MHz, 256MB NAND, 256MB DDR3, -40/+85C",
	},
/* 0x04 - 55001944-04 */
	{
		IMX6UL,
		MEM_1GB,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH,
		"Industrial Ultralite 528MHz, 1GB NAND, 1GB DDR3, -40/+85C, Wireless, Bluetooth",
	},
/* 0x05 - 55001944-05 */
	{
		IMX6UL,
		MEM_1GB,
		0,
		"Industrial Ultralite 528MHz, 1GB NAND, 1GB DDR3, -40/+85C",
	},
};

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	return 0;
}

#ifdef CONFIG_SYS_USE_NAND
static iomux_v3_cfg_t const nand_pads[] = {
	MX6_PAD_NAND_DATA00__RAWNAND_DATA00 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA01__RAWNAND_DATA01 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA02__RAWNAND_DATA02 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA03__RAWNAND_DATA03 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA04__RAWNAND_DATA04 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA05__RAWNAND_DATA05 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA06__RAWNAND_DATA06 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_DATA07__RAWNAND_DATA07 | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_CLE__RAWNAND_CLE | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_ALE__RAWNAND_ALE | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_CE0_B__RAWNAND_CE0_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_RE_B__RAWNAND_RE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_WE_B__RAWNAND_WE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_WP_B__RAWNAND_WP_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
	MX6_PAD_NAND_READY_B__RAWNAND_READY_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2),
};

static void setup_gpmi_nand(void)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/* config gpmi nand iomux */
	imx_iomux_v3_setup_multiple_pads(nand_pads, ARRAY_SIZE(nand_pads));

	clrbits_le32(&mxc_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);

	/*
	 * config gpmi and bch clock to 100 MHz
	 * bch/gpmi select PLL2 PFD2 400M
	 * 100M = 400M / 4
	 */
	clrbits_le32(&mxc_ccm->cscmr1,
		     MXC_CCM_CSCMR1_BCH_CLK_SEL |
		     MXC_CCM_CSCMR1_GPMI_CLK_SEL);
	clrsetbits_le32(&mxc_ccm->cscdr1,
			MXC_CCM_CSCDR1_BCH_PODF_MASK |
			MXC_CCM_CSCDR1_GPMI_PODF_MASK,
			(3 << MXC_CCM_CSCDR1_BCH_PODF_OFFSET) |
			(3 << MXC_CCM_CSCDR1_GPMI_PODF_OFFSET));

	/* enable gpmi and bch clock gating */
	setbits_le32(&mxc_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);

	/* enable apbh clock gating */
	setbits_le32(&mxc_ccm->CCGR0, MXC_CCM_CCGR0_APBHDMA_MASK);
}
#endif

#ifdef CONFIG_POWER
#define I2C_PMIC	0
static struct pmic *pfuze;
int power_init_ccimx6ul(void)
{
	int ret;
	unsigned int reg, rev_id;

	ret = power_pfuze3000_init(I2C_PMIC);
	if (ret)
		return ret;

	pfuze = pmic_get("PFUZE3000");
	ret = pmic_probe(pfuze);
	if (ret)
		return ret;

	pmic_reg_read(pfuze, PFUZE3000_DEVICEID, &reg);
	pmic_reg_read(pfuze, PFUZE3000_REVID, &rev_id);
	printf("PMIC:  PFUZE3000 DEV_ID=0x%x REV_ID=0x%x\n", reg, rev_id);

	/* disable Low Power Mode during standby mode */
	pmic_reg_read(pfuze, PFUZE3000_LDOGCTL, &reg);
	reg |= 0x1;
	pmic_reg_write(pfuze, PFUZE3000_LDOGCTL, reg);

	/* SW1A mode to APS/OFF, to switch off the regulator in standby */
	reg = 0x04;
	pmic_reg_write(pfuze, PFUZE3000_SW1AMODE, reg);

	/* SW1B step ramp up time from 2us to 4us/25mV */
	reg = 0x40;
	pmic_reg_write(pfuze, PFUZE3000_SW1BCONF, reg);

	/* SW1B mode to APS/PFM, to optimize performance */
	reg = 0xc;
	pmic_reg_write(pfuze, PFUZE3000_SW1BMODE, reg);

	/* SW1B voltage set to 1.3V */
	reg = 0x18;
	pmic_reg_write(pfuze, PFUZE3000_SW1BVOLT, reg);

	/* SW1B standby voltage set to 0.925V */
	reg = 0x09;
	pmic_reg_write(pfuze, PFUZE3000_SW1BSTBY, reg);

	/* SW2 mode to APS/OFF, to switch off in standby mode */
	reg = 0x04;
	pmic_reg_write(pfuze, PFUZE3000_SW2MODE, reg);

	return 0;
}

#ifdef CONFIG_LDO_BYPASS_CHECK
void ldo_mode_set(int ldo_bypass)
{
	unsigned int value;
	u32 vddarm;

	struct pmic *p = pfuze;

	if (!p) {
		printf("No PMIC found!\n");
		return;
	}

	/* switch to ldo_bypass mode */
	if (ldo_bypass) {
		prep_anatop_bypass();
		/* decrease VDDARM to 1.275V */
		pmic_reg_read(pfuze, PFUZE3000_SW1BVOLT, &value);
		value &= ~0x1f;
		value |= PFUZE3000_SW1AB_SETP(12750);
		pmic_reg_write(pfuze, PFUZE3000_SW1BVOLT, value);

		set_anatop_bypass(1);
		vddarm = PFUZE3000_SW1AB_SETP(11750);

		pmic_reg_read(pfuze, PFUZE3000_SW1BVOLT, &value);
		value &= ~0x1f;
		value |= vddarm;
		pmic_reg_write(pfuze, PFUZE3000_SW1BVOLT, value);

		finish_anatop_bypass();

		printf("switch to ldo_bypass mode!\n");
	}
}
#endif
#endif

void mca_init(void)
{
	unsigned char devid = 0;
	unsigned char hwver;
	unsigned char fwver[2];
	int ret, fwver_ret;

	ret = mca_read_reg(MCA_DEVICE_ID, &devid);
	if (devid != MCA_CC6UL_DEVICE_ID_VAL) {
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

int ccimx6ul_init(void)
{
#ifdef CONFIG_HAS_TRUSTFENCE
	uint32_t ret;
	uint8_t event_data[36] = { 0 }; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config = 0;
	enum hab_state state = 0;
	hab_rvt_report_status_t *hab_report_status = hab_rvt_report_status_p;

	/* HAB event verification */
	ret = hab_report_status(&config, &state);
	if (ret == HAB_WARNING) {
		pr_debug("\nHAB Configuration: 0x%02x, HAB State: 0x%02x\n",
		       config, state);
		/* Verify RNG self test */
		rng_swtest_status = hab_event_warning_check(event_data, &bytes);
		if (rng_swtest_status == SW_RNG_TEST_PASSED) {
			printf("RNG:   self-test failed, but software test passed.\n");
		} else if (rng_swtest_status == SW_RNG_TEST_FAILED) {
#ifdef CONFIG_RNG_SELF_TEST
			printf("WARNING: RNG self-test and software test failed!\n");
#else
			printf("WARNING: RNG self-test failed!\n");
#endif
			if (is_hab_enabled()) {
				printf("Aborting secure boot.\n");
				run_command("reset", 0);
			}
		}
	} else {
		rng_swtest_status = SW_RNG_TEST_NA;
	}
#endif /* CONFIG_HAS_TRUSTFENCE */

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

#ifdef CONFIG_SYS_I2C_MXC
	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c1_pad_info);
	mca_init();
#endif

#ifdef CONFIG_SYS_USE_NAND
	setup_gpmi_nand();
#endif

#ifdef CONFIG_MCA_TAMPER
	mca_tamper_check_events();
#endif
	return 0;
}

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"sd1", MAKE_CFGVAL(0x42, 0x20, 0x00, 0x00)},
	{"sd2", MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"qspi1", MAKE_CFGVAL(0x10, 0x00, 0x00, 0x00)},
	{"nand", MAKE_CFGVAL(0x90, 0x28, 0x00, 0x00)},
	{NULL,	 0},
};
#endif

void generate_partition_table(void)
{
	struct mtd_info *nand = nand_info[0];
	uint32_t nand_size_mb = nand->size / SZ_1M;

	switch (nand_size_mb) {
	case 1024:
		setenv("mtdparts", MTDPARTS_1024MB);
		break;
	case 256:
	default:
		setenv("mtdparts", MTDPARTS_256MB);
		break;
	}
}

void som_default_environment(void)
{
	char var[10];
	char *parttable;

	/* Partition table */
	parttable = getenv("mtdparts");
	if (!parttable)
		generate_partition_table();

	/* Set $module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	setenv("module_variant", var);
}

int ccimx6ul_late_init(void)
{
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
	gd->flags &= ~GD_FLG_DISABLE_CONSOLE_INPUT;
	if (!console_enable_passphrase())
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
	else
		gd->flags |= GD_FLG_DISABLE_CONSOLE_INPUT;
#endif

#ifdef CONFIG_HAS_TRUSTFENCE
	migrate_filesystem_key();
	copy_dek();
#endif

	/* Verify MAC addresses */
	verify_mac_address("ethaddr", DEFAULT_MAC_ETHADDR);
	verify_mac_address("eth1addr", DEFAULT_MAC_ETHADDR1);

	if (board_has_wireless())
		verify_mac_address("wlanaddr", DEFAULT_MAC_WLANADDR);

	if (board_has_bluetooth())
		verify_mac_address("btaddr", DEFAULT_MAC_BTADDR);

	return 0;
}

u32 get_board_rev(void)
{
	return get_cpu_rev();
}

static int is_valid_hwid(struct digi_hwid *hwid)
{
	if (hwid->variant < ARRAY_SIZE(ccimx6ul_variants))
		if (ccimx6ul_variants[hwid->variant].cpu != IMX6_NONE)
			return 1;

	return 0;
}

int board_has_wireless(void)
{
	if (is_valid_hwid(&my_hwid))
		return (ccimx6ul_variants[my_hwid.variant].capabilities &
				    CCIMX6_HAS_WIRELESS);
	else
		return 1; /* assume it has if invalid HWID */
}

int board_has_bluetooth(void)
{
	if (is_valid_hwid(&my_hwid))
		return (ccimx6ul_variants[my_hwid.variant].capabilities &
				    CCIMX6_HAS_BLUETOOTH);
	else
		return 1; /* assume it has if invalid HWID */
}

void print_ccimx6ul_info(void)
{
	if (is_valid_hwid(&my_hwid))
		printf("%s SOM variant 0x%02X: %s\n", CONFIG_SOM_DESCRIPTION,
			my_hwid.variant,
			ccimx6ul_variants[my_hwid.variant].id_string);
}

/*
 * This hook makes a custom reset implementation of the board that runs before
 * the standard cpu_reset() which does a CPU watchdog reset.
 * On the CC6UL we want instead to do an MCA watchdog reset, which will pull
 * down POR_B line, thus doing a controlled reset of the CPU.
 */
void board_reset(void)
{
	/*
	 * If a bmode_reset was flagged, do not reset through the MCA, which
	 * would otherwise power-cycle the CPU.
	 */
	if (bmode_reset)
		return;

	mca_reset();

	/*
	 * Give it some time to generate the reset, or else U-Boot will
	 * proceed with standard reset_cpu()
	 */
	mdelay(100);
}

void fdt_fixup_ccimx6ul(void *fdt)
{
	if (board_has_wireless()) {
		/* Wireless MACs */
		fdt_fixup_mac(fdt, "wlanaddr", "/wireless", "mac-address");
		fdt_fixup_mac(fdt, "wlan1addr", "/wireless", "mac-address1");
		fdt_fixup_mac(fdt, "wlan2addr", "/wireless", "mac-address2");
		fdt_fixup_mac(fdt, "wlan3addr", "/wireless", "mac-address3");

		/* Regulatory domain */
		fdt_fixup_regulatory(fdt);
	}

	if (board_has_bluetooth())
		fdt_fixup_mac(fdt, "btaddr", "/bluetooth", "mac-address");

	fdt_fixup_trustfence(fdt);
	fdt_fixup_uboot_info(fdt);
}
