#
# Copyright 2015-2018, Digi International Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# SPDX-License-Identifier:      GPL-2.0+
#

MINIMAL=

ifdef CONFIG_SPL_BUILD
ifdef CONFIG_SPL_INIT_MINIMAL
MINIMAL=y
endif
endif

ifdef MINIMAL
# necessary to create built-in.o
obj- := __dummy__.o
else
$(if $(CONFIG_CMD_BOOTSTREAM), $(shell mkdir -p $(obj)cmd_bootstream))

obj-$(CONFIG_CMD_BOOTSTREAM) += cmd_bootstream/bch.o \
				cmd_bootstream/ncb.o \
				cmd_bootstream/cmd_bootstream.o \
				cmd_bootstream/mxsboot.o
obj-$(CONFIG_MXS_OTP) += cmd_mxsotp.o
ifdef CONFIG_HAS_HWID
obj-y += cmd_hwid.o
obj-$(CONFIG_CC6) += ccimx6_hwid.o
endif
obj-$(CONFIG_HAS_CARRIERBOARD_VERSION) += cmd_board.o
obj-$(CONFIG_HAS_CARRIERBOARD_ID) += cmd_board.o
obj-$(CONFIG_AUTO_BOOTSCRIPT) += helper.o
obj-$(CONFIG_CMD_UPDATE_MMC) += cmd_update_mmc.o helper.o helper_mmc.o
obj-$(CONFIG_CMD_UPDATE_NAND) += cmd_update_nand.o helper.o helper_nand.o
obj-$(CONFIG_CMD_DBOOT) += cmd_dboot.o helper.o
obj-$(CONFIG_CMD_DIGI_PMIC) += cmd_pmic.o
obj-$(CONFIG_HAS_TRUSTFENCE) += cmd_trustfence.o
obj-$(CONFIG_MCA_CC6UL_TAMPER) += tamper.o
obj-$(CONFIG_USB_XHCI_IMX8) += ../../freescale/common/tcpc.o

endif
