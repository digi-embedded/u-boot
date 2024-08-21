#
# Copyright 2024 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ccimx91-dvk.o
obj-$(CONFIG_HAS_HWID) += ../ccimx9/ccimx9_hwid.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_CCIMX91_DVK_LPDDR4) += lpddr4_timing_512M.o
else
obj-y += ../ccimx9/ccimx9.o
obj-$(CONFIG_HAS_TRUSTFENCE) += ../ccimx9/ccimx9_trustfence.o ../common/trustfence/ahab.o
endif
