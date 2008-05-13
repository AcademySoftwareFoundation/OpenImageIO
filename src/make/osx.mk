# License and copyright goes here


# This file contains the Mac OS X specific macros
#
# This is included by platform.mk



CFLAGS += -DMACOSX -DOSX
# ? -fno-common

SHLIBEXT := .dylib
SHLIB_LDFLAGS := -dynamiclib

