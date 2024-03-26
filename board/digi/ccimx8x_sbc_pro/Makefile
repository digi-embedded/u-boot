# (C) Copyright 2018-2019 Digi International, Inc.
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y += ccimx8x_sbc_pro.o
obj-y += ../ccimx8x/ccimx8x.o \
	 ../ccimx8/ccimx8.o \
	 ../common/trustfence/ahab.o
ifdef CONFIG_HAS_HWID
obj-y += ../ccimx8/ccimx8_hwid.o
endif
obj-$(CONFIG_SPL_BUILD) += ../ccimx8x/spl.o
