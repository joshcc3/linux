# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 1999 - 2018 Intel Corporation.
#
# Makefile for the Intel(R) 82575 PCI-Express ethernet driver
#


ccflags-y += -c -g $(FLAGS) -Wall

# Apply C flags to the cpp compiler and disable cpp features that can't be supported in the kernel module
cxx-selected-flags = $(shell echo $(KBUILD_CFLAGS) \
            | sed s/-D\"KBUILD.\"//g \
            | sed s/-Werror=strict-prototypes//g \
            | sed s/-mno-sse//g \
            | sed s/-mno-sse2//g \
            | sed s/-Werror=implicit-function-declaration//g \
            | sed s/-Werror=implicit-int//g \
            | sed s/-Wdeclaration-after-statement//g \
            | sed s/-Wno-pointer-sign//g \
            | sed s/-Werror=incompatible-pointer-types//g \
            | sed s/-Werror=designated-init//g \
            | sed s/-Wreorder//g \
            | sed s/-mno-mmx\ 2/-mno-mmx/g \
            | sed s/-std=gnu11//g \
            )

cxxflags = $(ccflags-y) \
            $(cxx-selected-flags) \
            -fno-builtin \
            -nostdlib \
            -fno-rtti \
            -fno-exceptions \
            -std=c++17

HOSTCXX := g++

cxx-prefix := " $(FLAGS) | $(HOSTCXX) $(cxxflags) [M]  "


%.cpp.o: %.cpp
	@echo $(cxx-prefix)$@
	@$(HOSTCXX) $(cxxflags) $< -o $@
	@echo -n > $$(dirname $@)/.$$(basename $@).cmd


%.s.o: %.s
	as -g $< -o $@


obj-$(CONFIG_IGB) += igb.o


igb-objs := igb_main.o igb_ethtool.o e1000_82575.o \
	    e1000_mac.o e1000_nvm.o e1000_phy.o e1000_mbx.o \
	    e1000_i210.o igb_ptp.o igb_hwmon.o josh/cppkern/IGB82576Interop.o \
		josh/cppkern/ucontext_.s.o \
	    josh/cppkern/containers.cpp.o \
		josh/lowlatencylab/client/CoroutineMngr.cpp.o josh/lowlatencylab/client/L2OB.cpp.o josh/lowlatencylab/client/launch.cpp.o \
		josh/lowlatencylab/client/mdmcclient.cpp.o josh/lowlatencylab/client/Strat.cpp.o


#	     josh/containers.o \
#	    josh/CoroutineMngr.o josh/L2OB.o josh/launch.o \
#	    josh/mdmcclient.o josh/Strat.o

#josh/cppkern/test.o josh/cppkern/cppsupport.o
