#
# Copyright 2022 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ccimx93-dvk.o
obj-$(CONFIG_HAS_HWID) += ../ccimx93/ccimx93_hwid.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_CCIMX93_DVK_LPDDR4) += lpddr4_timing_1G.o lpddr4_timing_2G.o lpddr4_timing_512M.o
else
obj-y += ../ccimx93/ccimx93.o
obj-$(CONFIG_HAS_TRUSTFENCE) += ../ccimx93/ccimx93_trustfence.o ../common/trustfence/ahab.o
endif
