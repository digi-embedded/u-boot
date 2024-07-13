# SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
#
# Copyright (C) 2018, STMicroelectronics - All Rights Reserved
# Copyright (C) 2022-2024, Digi International Inc - All Rights Reserved
#

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += ccmp15-dvk.o
endif

ifdef CONFIG_HAS_HWID
obj-y += ../ccmp1/ccmp1_hwid.o
endif

obj-y += ../ccmp1/ccmp1.o
obj-$(CONFIG_CUSTOM_HUK) += ../ccmp1/ccmp15_huk.o

ifdef CONFIG_ENV_AES_CCMP1
obj-y += ../ccmp1/ta_ccmp1.o
endif

# ST common
obj-$(CONFIG_CMD_STBOARD) += ../../st/common/cmd_stboard.o
obj-$(CONFIG_PMIC_STPMIC1) += ../../st/common/stpmic1.o

ifeq ($(CONFIG_ARCH_STM32MP),y)
obj-$(CONFIG_SET_DFU_ALT_INFO) += ../../st/common/stm32mp_dfu.o
endif

