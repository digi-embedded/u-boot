// SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause
/*
 * Copyright (C) 2022, STMicroelectronics - All Rights Reserved
 */
#include <common.h>
#include <clk.h>
#include <cpu_func.h>
#include <debug_uart.h>
#include <env_internal.h>
#include <init.h>
#include <misc.h>
#include <wdt.h>
#include <asm/io.h>
#include <asm/arch/stm32.h>
#include <asm/arch/sys_proto.h>
#include <dm/device.h>
#include <dm/lists.h>
#include <dm/uclass.h>
#include <dt-bindings/clock/stm32mp25-clks.h>
#include <dt-bindings/pinctrl/stm32-pinfunc.h>

/*
 * early TLB into the .data section so that it not get cleared
 * with 16kB alignment
 */
#define EARLY_TLB_SIZE 0xA000
u8 early_tlb[EARLY_TLB_SIZE] __section(".data") __aligned(0x4000);

/*
 * initialize the MMU and activate cache in U-Boot pre-reloc stage
 * MMU/TLB is updated in enable_caches() for U-Boot after relocation
 */
static void early_enable_caches(void)
{
	if (CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
		return;

#if !(CONFIG_IS_ENABLED(SYS_ICACHE_OFF) && CONFIG_IS_ENABLED(SYS_DCACHE_OFF))
	gd->arch.tlb_size = EARLY_TLB_SIZE;
	gd->arch.tlb_addr = (unsigned long)&early_tlb;
#endif
	/* enable MMU (default configuration) */
	dcache_enable();
}

/*
 * Early system init
 */
int arch_cpu_init(void)
{
	icache_enable();
	early_enable_caches();

	return 0;
}

void enable_caches(void)
{
	/* deactivate the data cache, early enabled in arch_cpu_init() */
	dcache_disable();
	/*
	 * Force the call of setup_all_pgtables() in mmu_setup() by clearing tlb_fillptr
	 * to update the TLB location udpated in board_f.c::reserve_mmu
	 */
	gd->arch.tlb_fillptr = 0;
	dcache_enable();
}

/* used when CONFIG_DISPLAY_CPUINFO is activated */
int print_cpuinfo(void)
{
	char name[SOC_NAME_SIZE];

	get_soc_name(name);
	printf("CPU: %s\n", name);

	return 0;
}

u32 get_bootmode(void)
{
	/* read bootmode from TAMP backup register */
	return (readl(TAMP_BOOT_CONTEXT) & TAMP_BOOT_MODE_MASK) >>
		    TAMP_BOOT_MODE_SHIFT;
}

static void setup_boot_mode(void)
{
	env_set("boot_device", "mmc");
	env_set("boot_instance", "0");
}

int arch_misc_init(void)
{
	setup_boot_mode();

	return 0;
}
