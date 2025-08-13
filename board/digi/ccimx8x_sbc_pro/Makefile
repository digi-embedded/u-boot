# (C) Copyright 2018-2019 Digi International, Inc.
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y += ccimx8x_sbc_pro.o
obj-y += ../../freescale/common/mmc.o	# for "mmc_map_to_kernel_blk"
obj-y += ../ccimx8x/ccimx8x.o \
	 ../ccimx8/ccimx8.o
ifdef CONFIG_HAS_HWID
obj-y += ../ccimx8/ccimx8_hwid.o
endif

ifdef CONFIG_SPL_BUILD
obj-y += ../ccimx8x/spl.o
else
obj-$(CONFIG_HAS_TRUSTFENCE) += ../ccimx8x/ccimx8x_trustfence.o
endif
