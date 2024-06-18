# SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause
#
# Copyright (C) 2022, STMicroelectronics - All Rights Reserved
# Copyright (C) 2024, Digi International Inc - All Rights Reserved
#

obj-y += ccmp25-dvk.o

ifdef CONFIG_HAS_HWID
obj-y += ../ccmp2/ccmp2_hwid.o
endif

obj-y += ../ccmp2/ccmp2.o

# ST common
obj-$(CONFIG_CMD_STBOARD) += ../../st/common/cmd_stboard.o
obj-$(CONFIG_PMIC_STPMIC1) += ../../st/common/stpmic1.o

ifeq ($(CONFIG_ARCH_STM32MP),y)
obj-$(CONFIG_SET_DFU_ALT_INFO) += ../../st/common/stm32mp_dfu.o
obj-$(CONFIG_$(SPL_)DFU_VIRT) += ../../st/common/stm32mp_dfu_virt.o
endif
