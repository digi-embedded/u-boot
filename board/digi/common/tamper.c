/*
 *  Copyright (C) 2016 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#include <asm/global_data.h>
#include <common.h>
#include <linux/delay.h>
#include <linux/libfdt.h>
#include "../common/mca_registers.h"
#include "../common/tamper.h"

DECLARE_GLOBAL_DATA_PTR;

extern int mca_bulk_read(int reg, unsigned char *values, int len);
extern int mca_write_reg(int reg, unsigned char value);

__weak void mca_tamper_take_actions(int iface)
{
	/* Override this function with a custom implementation... */
	printf("\nTamper%d Event!!\n", iface);
	printf("No specific action was implemented\n\n");
}

#ifdef MCA_TAMPER_USE_DT
static int mca_tamper_iface_enabled_in_dt(int iface)
{
	const int *tamper_iface_prop;
	int iface_enabled = 0, tamper_iface;
	int node;

	/* get the right fdt_blob from the global working_fdt */
	gd->fdt_blob = working_fdt;

	/* Get the node from FDT for mca tamper */
	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1,
					     "digi,mca-cc6ul-tamper");
	if (node < 0) {
#ifdef MCA_TAMPER_DEBUG
		printf("No mca tamper device node %d, force to disable\n", node);
#endif
		return 0;
	}

	for (; node > 0; node = fdt_next_property_offset(gd->fdt_blob, node)) {

		tamper_iface_prop = fdt_getprop(gd->fdt_blob, node,
						"digi,tamper-if-list", NULL);
		if (!tamper_iface_prop)
			continue;

		tamper_iface = fdt32_to_cpu(*tamper_iface_prop);
		if (tamper_iface == iface) {
			iface_enabled = 1;
			break;
		}
	}
#ifdef MCA_TAMPER_DEBUG
	printf("Tamper%d %s\n", iface, iface_enabled ? "enabled" : "disabled");
#endif

	return iface_enabled;
}
#endif

static unsigned int get_tamper_base_reg(unsigned int iface)
{
	switch (iface) {
	case 0:
		return MCA_TAMPER0_CFG0;
	case 1:
		return MCA_TAMPER1_CFG0;
	case 2:
		return MCA_TAMPER2_CFG0;
	case 3:
		return MCA_TAMPER3_CFG0;
	default:
		printf("MCA: tamper interface %d not supported\n", iface);
		break;
	}

	return ~0;
}

static void mca_tamper_ack_event(int iface)
{
	int ret;
	unsigned int regaddr = get_tamper_base_reg(iface);

	if (regaddr == ~0)
		return;


	/* Ack the tamper event */
	ret = mca_write_reg(regaddr + MCA_TAMPER_EV_OFFSET,
			    MCA_TAMPER_ACKED);
	if (ret)
		printf("MCA: unable to write Tamper%d event register\n", iface);
	mdelay(100);
}

static int mca_tamper_check_event(int iface)
{
	unsigned char data[MCA_TAMPER_REG_LEN];
	int ret;
	unsigned int regaddr = get_tamper_base_reg(iface);

	if (regaddr == ~0)
		return 0;

	ret = mca_bulk_read(regaddr, data, MCA_TAMPER_REG_LEN);
	if (ret) {
		printf("MCA: unable to read Tamper%d registers\n", iface);
		/* Return a tamper condition as the MCA could have been tampered */
		return 1;
	}

	/* Skip if tamper pin is not enabled */
	if (!(data[MCA_TAMPER_CFG0_OFFSET] & MCA_TAMPER_DET_EN))
		return 0;

	/* Check if there is an event signaled that has not been acked */
	if (data[MCA_TAMPER_EV_OFFSET] & MCA_TAMPER_SIGNALED &&
	   !(data[MCA_TAMPER_EV_OFFSET] & MCA_TAMPER_ACKED))
		return 1;

	return 0;
}

void mca_tamper_check_events(void)
{
	int i;

	for (i = 0; i < MCA_MAX_TAMPER_PINS; i++) {
#ifdef MCA_TAMPER_USE_DT
		/* Check if interface is enabled in DT */
		if (!mca_tamper_iface_enabled_in_dt(i))
			continue;
#endif
		/* Check if there is a tamper event signaled and not acked */
		if (!mca_tamper_check_event(i))
			continue;

		/* Take the neccesary actions */
		mca_tamper_take_actions(i);

		/* And finally ack the event */
		mca_tamper_ack_event(i);
	}
}
