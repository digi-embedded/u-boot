// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023-2025 NXP
 *
 */

#include <asm/io.h>
#include <clk-uclass.h>
#include <clk/scmi.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dt-bindings/clock/nxp,imx95-clock.h>
#include <imx95-clock.h>
#include <dt-bindings/clock/nxp,imx94-clock.h>
#include <imx94-clock.h>
#include <linux/clk-provider.h>

#include "clk.h"

enum {
	CLK_GATE,
	CLK_DIVIDER,
	CLK_MUX,
};

#define MAX_NUM_PARENTS 4
struct imx95_blk_ctl_clk_dev_data {
	const char *name;
	const char * const *parent_names;
	u32 clk_parent_ids[4];
	u32 num_parents;
	u32 reg;
	u32 bit_idx;
	u32 flags;
	u32 flags2;
	u32 type;
};

struct imx95_blk_ctl_dev_data {
	const struct imx95_blk_ctl_clk_dev_data *clk_dev_data;
	u32 num_clks;
	u32 clk_reg_offset;
};

static const struct imx95_blk_ctl_clk_dev_data hsio_blk_ctl_clk_dev_data[] = {
	[0] = {
		.name = "hsio_blk_ctl_clk",
		.parent_names = (const char *[]){ "hsiopll", },
		.clk_parent_ids = { IMX95_CLK_HSIOPLL, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 6,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
	},
	[1] = {
		.name = "hsio_blk_ctl_clk1",
		.parent_names = (const char *[]){ "hsiopll", },
		.clk_parent_ids = { IMX95_CLK_HSIOPLL, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 2,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
	}
};

static const struct imx95_blk_ctl_dev_data hsio_blk_ctl_dev_data = {
	.num_clks = ARRAY_SIZE(hsio_blk_ctl_clk_dev_data),
	.clk_dev_data = hsio_blk_ctl_clk_dev_data,
	.clk_reg_offset = 0,
};

static const struct imx95_blk_ctl_clk_dev_data imx95_lvds_clk_dev_data[] = {
	[IMX95_CLK_DISPMIX_LVDS_PHY_DIV] = {
		.name = "ldb_phy_div",
		.parent_names = (const char *[]){ "ldbpll", },
		.clk_parent_ids = { IMX95_CLK_LDBPLL, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 0,
		.type = CLK_DIVIDER,
		.flags2 = CLK_DIVIDER_POWER_OF_TWO,
	},

	[IMX95_CLK_DISPMIX_LVDS_CH0_GATE] = {
		.name = "lvds_ch0_gate",
		.parent_names = (const char *[]){ "ldb_phy_div", },
		.clk_parent_ids = { ~0U, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 1,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
		.flags2 = CLK_GATE_SET_TO_DISABLE,
	},
	[IMX95_CLK_DISPMIX_LVDS_CH1_GATE] = {
		.name = "lvds_ch1_gate",
		.parent_names = (const char *[]){ "ldb_phy_div", },
		.clk_parent_ids = { ~0U, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 2,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
		.flags2 = CLK_GATE_SET_TO_DISABLE,
	},
	[IMX95_CLK_DISPMIX_PIX_DI0_GATE] = {
		.name = "lvds_di0_gate",
		.parent_names = (const char *[]){ "ldb_pll_div7", },
		.clk_parent_ids = { ~0U, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 3,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
		.flags2 = CLK_GATE_SET_TO_DISABLE,
	},
	[IMX95_CLK_DISPMIX_PIX_DI1_GATE] = {
		.name = "lvds_di1_gate",
		.parent_names = (const char *[]){ "ldb_pll_div7", },
		.clk_parent_ids = { ~0U, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 4,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
		.flags2 = CLK_GATE_SET_TO_DISABLE,
	},
};

static const struct imx95_blk_ctl_dev_data imx95_lvds_csr_dev_data = {
	.num_clks = ARRAY_SIZE(imx95_lvds_clk_dev_data),
	.clk_dev_data = imx95_lvds_clk_dev_data,
	.clk_reg_offset = 0,
};

static const struct imx95_blk_ctl_clk_dev_data imx94_lvds_clk_dev_data[] = {
	[IMX94_CLK_DISPMIX_LVDS_CLK_GATE] = {
		.name = "lvds_clk_gate",
		.parent_names = (const char *[]){ "ldb_pll_div7", },
		.clk_parent_ids = { ~0U, },
		.num_parents = 1,
		.reg = 0,
		.bit_idx = 1,
		.type = CLK_GATE,
		.flags = CLK_SET_RATE_PARENT,
		.flags2 = CLK_GATE_SET_TO_DISABLE,
	},
};

static const struct imx95_blk_ctl_dev_data imx94_lvds_csr_dev_data = {
	.num_clks = ARRAY_SIZE(imx94_lvds_clk_dev_data),
	.clk_dev_data = imx94_lvds_clk_dev_data,
};

static const char * const imx94_disp_engine_parents[] = {
	"disppix", "ldb_pll_div7"
};

static const struct imx95_blk_ctl_clk_dev_data imx94_dispmix_csr_clk_dev_data[] = {
	[IMX94_CLK_DISPMIX_CLK_SEL] = {
		.name = "disp_clk_sel",
		.parent_names = imx94_disp_engine_parents,
		.clk_parent_ids = { IMX94_CLK_DISPPIX, ~0U, },
		.num_parents = ARRAY_SIZE(imx94_disp_engine_parents),
		.reg = 0,
		.bit_idx = 1,
		.type = CLK_MUX,
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT,
	},
};

static const struct imx95_blk_ctl_dev_data imx94_dispmix_csr_dev_data = {
	.num_clks = ARRAY_SIZE(imx94_dispmix_csr_clk_dev_data),
	.clk_dev_data = imx94_dispmix_csr_clk_dev_data,
};

static int imx95_blkctrl_clk_probe(struct udevice *dev)
{
	int i, j, ret;
	void __iomem *addr;
	struct imx95_blk_ctl_dev_data *dev_data = (void *)dev_get_driver_data(dev);
	const struct imx95_blk_ctl_clk_dev_data *clk_dev_data;
	struct udevice *scmi_clk_dev;

	addr = dev_read_addr_ptr(dev);
	if (addr == (void *)FDT_ADDR_T_NONE) {
		dev_err(dev, "No blkctrl register base address\n");
		return -EINVAL;
	}

	if (!dev_data) {
		dev_err(dev, "driver data is NULL\n");
		return -EINVAL;
	}

	ret = uclass_get_device_by_name(UCLASS_CLK, "protocol@14", &scmi_clk_dev);
	if (ret) {
		dev_err(dev, "Failed to find procotol@14\n");
		return ret;
	}

	clk_dev_data = dev_data->clk_dev_data;
	for (i = 0; i < dev_data->num_clks; i++) {
		ulong id;
		if (clk_dev_data[i].type == CLK_GATE ||
		    clk_dev_data[i].type == CLK_DIVIDER) {
			if (clk_dev_data[i].clk_parent_ids[0] == ~0U)
				continue;

			id = CLK_ID(scmi_clk_dev, clk_dev_data[i].clk_parent_ids[0]);
			ret = scmi_clk_resolve_attr(id, NULL);
			if (ret) {
				dev_err(dev, "Failed to resolve clk %s\n",
					clk_dev_data[i].parent_names[0]);
				return ret;
			}
		} else if (clk_dev_data[i].type == CLK_MUX) {
			for (j = 0; j < clk_dev_data[i].num_parents; j++) {
				if (clk_dev_data[i].clk_parent_ids[j] == ~0U)
					continue;
				id = CLK_ID(scmi_clk_dev, clk_dev_data[i].clk_parent_ids[j]);
				ret = scmi_clk_resolve_attr(id, NULL);
				if (ret) {
					dev_err(dev, "Failed to resolve clk %s\n",
						clk_dev_data[i].parent_names[0]);
					return ret;
				}
			}
		}
	}

	for (i = 0; i < dev_data->num_clks; i++) {
		if (clk_dev_data[i].type == CLK_GATE) {
			dev_clk_dm(dev, i,
				   clk_register_gate(dev,
						     clk_dev_data[i].name,
						     clk_dev_data[i].parent_names[0],
						     clk_dev_data[i].flags, addr +
						     dev_data->clk_reg_offset,
						     clk_dev_data[i].bit_idx,
						     clk_dev_data[i].flags2, NULL));
		} else if (clk_dev_data[i].type == CLK_DIVIDER) {
			dev_clk_dm(dev, i,
				   clk_register_divider(dev, clk_dev_data[i].name,
							clk_dev_data[i].parent_names[0],
							clk_dev_data[i].flags, addr +
							dev_data->clk_reg_offset,
							clk_dev_data[i].bit_idx, 1,
							clk_dev_data[i].flags2));
		} else if (clk_dev_data[i].type == CLK_MUX) {
			dev_clk_dm(dev, i,
				   clk_register_mux(dev,
						    clk_dev_data[i].name,
						    clk_dev_data[i].parent_names,
						    clk_dev_data[i].num_parents,
						    clk_dev_data[i].flags, addr +
						    dev_data->clk_reg_offset,
						    clk_dev_data[i].bit_idx, 1,
						    clk_dev_data[i].flags2));
		}
	}

	return 0;
}

static const struct udevice_id imx95_blkctrl_clk_ids[] = {
	{ .compatible = "nxp,imx94-display-csr", .data = (ulong)&imx94_dispmix_csr_dev_data },
	{ .compatible = "nxp,imx94-lvds-csr", .data = (ulong)&imx94_lvds_csr_dev_data },
	{ .compatible = "nxp,imx95-lvds-csr", .data = (ulong)&imx95_lvds_csr_dev_data, },
	{ .compatible = "nxp,imx95-hsio-blk-ctl", .data = (ulong)&hsio_blk_ctl_dev_data,  },
	{ },
};

U_BOOT_DRIVER(imx95_blkctrl_clk) = {
	.name = "imx95_blkctrl_clk",
	.id = UCLASS_CLK,
	.of_match = imx95_blkctrl_clk_ids,
	.ops = &ccf_clk_ops,
	.probe = imx95_blkctrl_clk_probe,
#if CONFIG_IS_ENABLED(OF_REAL)
	.bind = dm_scan_fdt_dev,
#endif
	.flags = DM_FLAG_PRE_RELOC,
};
