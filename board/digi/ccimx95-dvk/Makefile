#
# Copyright 2025 Digi International Inc
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += ccimx95-dvk.o
obj-$(CONFIG_HAS_HWID) += ../ccimx9/ccimx9_hwid.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += ../ccimx9/ccimx9.o
obj-$(CONFIG_HAS_TRUSTFENCE) += ../ccimx9/ccimx9_trustfence.o
obj-$(CONFIG_USB_TCPC) += ../../freescale/common/tcpc.o
endif
