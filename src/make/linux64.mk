# License and copyright goes here


# This file contains the Linux-specific macros
#
# This is included by platform.mk



CFLAGS += -DLINUX -DLINUX64 
# 64-bit Linux should compile using PIC code
CFLAGS += -fPIC

CP := cp -uvpf
