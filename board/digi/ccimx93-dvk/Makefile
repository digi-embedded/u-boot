#
# Copyright 2022 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ccimx93-dvk.o
ifdef CONFIG_HAS_HWID
obj-y += ../ccimx93/ccimx93_hwid.o
endif

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_CCIMX93_DVK_LPDDR4X) += lpddr4x_timing.o
else
obj-y += ../ccimx93/ccimx93.o
endif
