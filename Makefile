# SPDX-License-Identifier: GPL-2.0
#

ifneq ($(KERNELRELEASE),)

	obj-m  = gpio-pulse-counter.o

else
	MODULE_DIR := $(shell pwd)

.PHONY: all
all: modules

.PHONY: modules
modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODULE_DIR)  modules

.PHONY: modules_install
modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODULE_DIR)  modules_install

.PHONY: clean
clean:
	rm -f *.o *.ko *.mod.c .*.o .*.o.d .*.ko .*.mod.c .*.cmd *~ *.ko.unsigned *.mod
	rm -f Module.symvers Module.markers modules.order
	rm -rf .tmp_versions .cache.mk

endif

