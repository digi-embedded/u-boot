#
# Copyright 2022 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ../ccimx8m_dvk/ccimx8m_dvk.o

ifdef CONFIG_HAS_HWID
obj-y += ../ccimx8/ccimx8_hwid.o
endif

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_IMX8M_LPDDR4) += ../ccimx8m/ccimx8mp_lpddr4_timing.o
else
obj-y += ../ccimx8/ccimx8.o ../ccimx8m/ccimx8m.o
endif
