/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <asm/global_data.h>
#include <fdtdec.h>
#include <common.h>
#include "fsl_sec.h"

#define CAAM_BASE 		CFG_SYS_FSL_SEC_ADDR
typedef int (*workaround_entry_t)(void * caamBase, unsigned long ent_dly,
            int state_handle_mask, int gen_sk, int reseed);

static bool copy_success = false;
DECLARE_GLOBAL_DATA_PTR;

static int rng_get_sram(int node)
{
	u32 *sram, *reg;
	int len;
	int sram_node;

	sram = (u32 *)fdt_getprop(gd->fdt_blob, node, "rng_workaround_sram", &len);
	if (!sram || len < 0) {
        printf("Unable to get sram\n");
        return -FDT_ERR_NOTFOUND;
    }

	sram_node = fdt_node_offset_by_phandle(gd->fdt_blob, fdt32_to_cpu(*sram));
	if (sram_node < 0) {
        printf("Unable to get sram node offset\n");
        return -FDT_ERR_NOTFOUND;
    }

	reg = (u32 *)fdt_getprop(gd->fdt_blob, sram_node, "reg", &len);
	if (!reg || len < 0) {
		printf("Unable to get sram reg\n");
		return -FDT_ERR_NOTFOUND;
	}

	return fdt32_to_cpu(reg[1]);
}

/*
 * rng_get_workaround_bin()
 */
static int rng_get_workaround_bin(int node, void **phtest, int *phtest_len)
{
	const char *subnode_name = "workaround-bin";
	const void *bin;
	int len;

	bin = fdt_getprop(gd->fdt_blob, node, subnode_name, &len);
	if (!bin || len < 0) {
		printf("Unable to get workaround-bin\n");
		*phtest = NULL;
		*phtest_len = 0;
		return -FDT_ERR_NOTFOUND;
	}

	*phtest = (void *)bin;
	*phtest_len = len;

	return 0;
}

#ifdef CONFIG_FSL_CAAM
/*
 * run the workaround code from OCRAM
 */
int rng_workaround_run(unsigned long ent_dly,
            int state_handle_mask, int gen_sk, int reseed){
	const char *node_name = "/soc/rng_workaround";
	int node;
	int sram_addr;
	workaround_entry_t apply_rng_workaround;
	int ret;
	ccsr_sec_t __iomem *sec = (ccsr_sec_t __iomem *)CAAM_BASE;
	struct rng4tst __iomem *rng =
			(struct rng4tst __iomem *)&sec->rng;

	if(!copy_success){
		ret = rng_workaround_copy_to_ocram();
		if(ret){
			printf("copy binary failed\n");
			return ret;
		}
	}
	
	node = fdt_path_offset(gd->fdt_blob, node_name);
	if (node < 0) {
		printf("Unable to get crypto node offset\n");
		return node;
	}

	sram_addr = rng_get_sram(node);
	if (sram_addr < 0) {
		printf("Unable to read sram addr\n");
		return sram_addr;
	}

	apply_rng_workaround = (workaround_entry_t)((char*)sram_addr + 1);

	debug("before run workaround, rdsta: 0x%X\n",sec_in32(&rng->rdsta));

	ret = apply_rng_workaround((void *)CAAM_BASE, ent_dly, state_handle_mask, gen_sk, reseed);
	if(ret){
		printf("Entropy delay = %lu, workaround failed with ret:%d\n",ent_dly, ret);
		return ret;
	}else{
		printf("Entropy delay = %lu, rng workaround init success\n",ent_dly);
	}

	return 0;
}

#endif

/*
 * copy the standalone workaround code from dtb to OCRAM region
 */
int rng_workaround_copy_to_ocram(void)
{
	const char *node_name = "/soc/rng_workaround";
	int node;
	void *source;
	int source_len;
	int sram_addr;
	int ret;

	if(copy_success){
		return 0;
	}

	printf("Loading rng workaround fw: ");
	
	node = fdt_path_offset(gd->fdt_blob, node_name);
	if (node < 0) {
		printf("Unable to get crypto node offset - ");
		return node;
	}

	ret = rng_get_workaround_bin(node, &source, &source_len);
	if (ret < 0) {
		printf("Unable to find binary data - ");
		return ret;
	}

	sram_addr = rng_get_sram(node);
	if (sram_addr < 0) {
		printf("Unable to read sram addr - \n");
		return sram_addr;
	}

	memcpy((void*)sram_addr, source, source_len);
	copy_success = true;
	printf("Success - %d bytes to 0x%X\n",source_len, sram_addr);

	return 0;
}
