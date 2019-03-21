/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <asm/arch/clock.h>
#include <mapmem.h>
#include <malloc.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include <fsl_sec.h>
#include "jobdesc.h"
#include "desc.h"
#include "jr.h"

DECLARE_GLOBAL_DATA_PTR;

uint32_t rng_dsc1[] = {
	0xb0800036, 0x04800010, 0x3c85a15b, 0x50a9d0b1,
	0x71a09fee, 0x2eecf20b, 0x02800020, 0xb267292e,
	0x85bf712d, 0xe85ff43a, 0xa716b7fb, 0xc40bb528,
	0x27b6f564, 0x8821cb5d, 0x9b5f6c26, 0x12a00020,
	0x0a20de17, 0x6529357e, 0x316277ab, 0x2846254e,
	0x34d23ba5, 0x6f5e9c32, 0x7abdc1bb, 0x0197a385,
	0x82500405, 0xa2000001, 0x10880004, 0x00000005,
	0x12820004, 0x00000020, 0x82500001, 0xa2000001,
	0x10880004, 0x40000045, 0x02800020, 0x8f389cc7,
	0xe7f7cbb0, 0x6bf2073d, 0xfc380b6d, 0xb22e9d1a,
	0xee64fcb7, 0xa2b48d49, 0xdf9bc3a4, 0x82500009,
	0xa2000001, 0x10880004, 0x00000005, 0x82500001,
	0x60340020, 0xFFFFFFFF, 0xa2000001, 0x10880004,
	0x00000005, 0x8250000d
};

uint32_t rng_result1[] = {
	0x3a, 0xfe, 0x2c, 0x87, 0xcc, 0xb6, 0x44, 0x49,
	0x19, 0x16, 0x9a, 0x74, 0xa1, 0x31, 0x8b, 0xef,
	0xf4, 0x86, 0x0b, 0xb9, 0x5e, 0xee, 0xae, 0x91,
	0x92, 0xf4, 0xa9, 0x8f, 0xb0, 0x37, 0x18, 0xa4
};

uint32_t rng_dsc2[] = {
	0xb080003a, 0x04800020, 0x27b73130, 0x30b4b10f,
	0x7c62b1ad, 0x77abe899, 0x67452301, 0xefcdab89,
	0x98badcfe, 0x10325476, 0x02800020, 0x63f757cf,
	0xb9165584, 0xc3c1b407, 0xcc4ce8ad, 0x1ffe8a58,
	0xfb4fa893, 0xbb5f4af0, 0x3fb946a1, 0x12a00020,
	0x56cbcaa5, 0xfff3adad, 0xe804dcbf, 0x9a900c71,
	0xa42017e3, 0x826948e2, 0xd0cfeb3e, 0xaf1a136a,
	0x82500405, 0xa2000001, 0x10880004, 0x00000005,
	0x12820004, 0x00000020, 0x82500001, 0xa2000001,
	0x10880004, 0x40000045, 0x02800020, 0x2e882f8a,
	0xe929943e, 0x8132c0a8, 0x12037f90, 0x809fbd66,
	0x8684ea04, 0x00cbafa7, 0x7b82d12a, 0x82500009,
	0xa2000001, 0x10880004, 0x00000005, 0x82500001,
	0x60340020, 0xFFFFFFFF, 0xa2000001, 0x10880004,
	0x00000005, 0x8250000d
};

uint32_t rng_result2[] = {
	0x76, 0x87, 0x66, 0x4e, 0xd8, 0x1d, 0x1f, 0x43,
	0x76, 0x50, 0x85, 0x5d, 0x1e, 0x1d, 0x9d, 0x0f,
	0x93, 0x75, 0x83, 0xff, 0x9a, 0x9b, 0x61, 0xa9,
	0xa5, 0xeb, 0xa3, 0x28, 0x2a, 0x15, 0xc1, 0x57
};

#define DSC1SIZE 0x36
#define DSC2SIZE 0x3A
#define KAT_SIZE 32

#define CAAM_ORSR_JRa_OFFSET	0x102c
#define CAAM_CCBVID_OFFSET	0x0FE4
#define CAAM_CHAVID_LS_OFFSET	0x0FEC
#define CAAM_CRNR_LS_OFFSET	0x0FA4
#define RNGVID_MASK		0x000F
#define RNGRN_MASK		0x000F
#define CAAM_ERA_MASK		0xFF

/*
 * construct_rng_self_test_jobdesc() - Implement destination address in RNG self test descriptors
 * @result:	RNG descriptors constructed
 *
 * Returns zero on success,and negative on error.
 */
int construct_rng_self_test_jobdesc(u32 *desc, u32 *rng_st_dsc, u8 *res_addr, int desc_size)
{
	int result_addr_idx = desc_size - 5;
	int i = 0;

	/* Replace destination address in the descriptor */
	rng_st_dsc[result_addr_idx] = (u32)res_addr;

	for (i = 0; i < desc_size; i++)
		desc[i] = rng_st_dsc[i];

	pr_debug("RNG SELF TEST DESCRIPTOR:\n");
	for (i = 0; i < desc_size; i++)
		pr_debug("0x%08X\n", desc[i]);
	pr_debug("\n");

	if (!desc) {
		puts("RNG self test descriptor construction failed\n");
		return -1;
	}

	return 0;
}

/*
 * rng_self_test() - Perform RNG self test
 * @result:	The address where RNG self test will reside in memory
 *
 * Returns zero on success,and negative on error.
 */
int rng_self_test(u8 *result)
{
	int ret, size, i, desc_size = 0;
	u8 caam_era, rngvid, rngrev;
	u32 *rng_st_dsc, *exp_result, *desc;
	u32 jr_size = 4, out_jr_size;

	caam_era = (CAAM_ERA_MASK & (sec_in32(CONFIG_SYS_FSL_SEC_ADDR + CAAM_CCBVID_OFFSET) >> 24));
	rngvid = (RNGVID_MASK & (sec_in32(CONFIG_SYS_FSL_SEC_ADDR + CAAM_CHAVID_LS_OFFSET) >> 16));
	rngrev = (RNGRN_MASK & (sec_in32(CONFIG_SYS_FSL_SEC_ADDR + CAAM_CRNR_LS_OFFSET) >> 16));

	if (caam_era < 8 && rngvid == 4 && rngrev < 3) {
		rng_st_dsc = (uint32_t *)rng_dsc1;
		desc_size = DSC1SIZE;
		exp_result = (uint32_t *)rng_result1;
	} else if (rngvid != 3) {
		rng_st_dsc = (uint32_t *)rng_dsc2;
		desc_size = DSC2SIZE;
		exp_result = (uint32_t *)rng_result2;
	} else {
		puts("Error : Invalid CAAM ERA or RNG Version ID or RNG revision\n");
		return -1;
	}

	out_jr_size = sec_in32(CONFIG_SYS_FSL_JR0_ADDR + CAAM_ORSR_JRa_OFFSET);
	if (out_jr_size != jr_size) {
		hab_caam_clock_enable(1);
		sec_init();
	}

	desc = memalign(ARCH_DMA_MINALIGN, sizeof(uint32_t) * desc_size);
	if (!desc) {
		puts("Not enough memory for descriptor allocation\n");
		return -ENOMEM;
	}

	ret = construct_rng_self_test_jobdesc(desc, rng_st_dsc, result, desc_size);
	if (ret) {
		puts("Error in Job Descriptor Construction\n");
		goto err;
	} else {
		size = roundup(sizeof(uint32_t) * desc_size, ARCH_DMA_MINALIGN);
		flush_dcache_range((unsigned long)desc, (unsigned long)desc + size);
		size = roundup(sizeof(uint8_t) * KAT_SIZE, ARCH_DMA_MINALIGN);
		flush_dcache_range((unsigned long)result, (unsigned long)result + size);

		ret = run_descriptor_jr(desc);
	}

	if (ret) {
		printf("Error while running RNG self-test descriptor: %d\n", ret);
		goto err;
	}

	size = roundup(KAT_SIZE, ARCH_DMA_MINALIGN);
	invalidate_dcache_range((unsigned long)result, (unsigned long)result + size);

	pr_debug("Result\n");
	for (i = 0; i < KAT_SIZE; i++)
		pr_debug("%02X", result[i]);
	pr_debug("\n");

	pr_debug("Expected Result\n");
	for (i = 0; i < KAT_SIZE; i++)
		pr_debug("%02X", exp_result[i]);
	pr_debug("\n");

	for (i = 0; i < KAT_SIZE; i++) {
		if (result[i] != exp_result[i]) {
			pr_debug("RNG self test failed\n");
			ret = -1;
			goto err;
		}
	}
	pr_debug("RNG self test passed\n");

err:
	free(desc);
	return ret;
}
