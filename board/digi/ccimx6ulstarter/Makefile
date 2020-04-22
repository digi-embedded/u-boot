# (C) Copyright 2016-2019 Digi International, Inc.
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y  := ccimx6ulstarter.o ../ccimx6ul/ccimx6ul.o
ifdef CONFIG_HAS_HWID
obj-y += ../ccimx6/ccimx6_hwid.o
endif

extra-$(CONFIG_USE_PLUGIN) :=  ../ccimx6ul/plugin.bin
$(obj)/../ccimx6ul/plugin.bin: $(obj)/../ccimx6ul/plugin.o
	$(OBJCOPY) -O binary --gap-fill 0xff $< $@
