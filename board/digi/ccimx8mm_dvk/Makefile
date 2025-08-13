#
# Copyright 2021 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ../ccimx8m_dvk/ccimx8m_dvk.o
obj-y += ../../freescale/common/mmc.o	# for "mmc_map_to_kernel_blk"
ifdef CONFIG_HAS_HWID
obj-y += ../ccimx8/ccimx8_hwid.o
endif

ifndef CONFIG_SPL_BUILD
obj-y += ../ccimx8m/ccimx8m.o \
	 ../ccimx8/ccimx8.o
obj-$(CONFIG_HAS_TRUSTFENCE) += ../ccimx8m/ccimx8m_trustfence.o
else
obj-y += ../ccimx8m/spl.o
obj-$(CONFIG_IMX8M_LPDDR4) += ../ccimx8m/ccimx8mm_lpddr4_timing.o
endif
