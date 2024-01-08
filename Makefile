# SPDX-License-Identifier: GPL-2.0
#

ifneq ($(KERNELRELEASE),)

	obj-m  = savelec-pulse-counter.o

else

	export ARCH          := arm
	export KERNEL_DIR    := ~/builds/build-azlac-vip-iii/tmp/work/azlac_vip_iii-poky-linux-gnueabi/linux-mainline/4.14.213-phy1-r0.0/build/

	MODULE_DIR := $(shell pwd)

.PHONY: all
all: modules

.PHONY: modules
modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(MODULE_DIR)  modules

.PHONY: clean
clean:
	rm -f *.o *.ko *.mod.c .*.o .*.o.d .*.ko .*.mod.c .*.cmd *~ *.ko.unsigned *.mod
	rm -f Module.symvers Module.markers modules.order
	rm -rf .tmp_versions .cache.mk

endif

