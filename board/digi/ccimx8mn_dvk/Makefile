#
# Copyright 2019 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ccimx8mn_dvk.o
obj-y += ../ccimx8m/ccimx8m.o \
	 ../ccimx8/ccimx8.o
ifdef CONFIG_HAS_HWID
obj-y += ../ccimx8/ccimx8_hwid.o
endif

ifdef CONFIG_SPL_BUILD
obj-y += ../ccimx8m/spl.o
obj-$(CONFIG_IMX8M_LPDDR4) += ../ccimx8m/lpddr4_timing.o
endif
