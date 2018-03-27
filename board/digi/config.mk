CPPFLAGS-y = \
	-DUBOOT \
	-I$(TOPDIR)/board/$(VENDOR)/common

CPPFLAGS-$(CONFIG_CMD_BOOTSTREAM) += \
	-I$(TOPDIR)/board/$(VENDOR)/common/cmd_bootstream \
	-I$(TOPDIR)/drivers/mtd/nand

CPPFLAGS += $(CPPFLAGS-y)
ccflags-y += $(CPPFLAGS-y)
