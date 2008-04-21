# License and copyright goes here


#########################################################################
# platform.mk
#
# Figure out which platform we are building on/for, set ${platform} and
# ${hw}, and include the relevant platform-specific makefiles.
#
# This is called from master.mk
#########################################################################


#########################################################################
# Figure out which platform we are building on/for

# Start with unknown platform
platform := "unknown"

# Use 'uname -m' to determine the hardware architecture.  This should
# return "x86" or "x86_64"
hw := ${shell uname -m}
$(info hardware = ${hw})
ifneq (${hw},x86)
    ifneq (${hw},x86_64)
	error "ERROR: Unknown hardware architecture"
    endif
endif

# Use 'uname', lowercased and stripped of pesky stuff, and the hardware
# architecture in ${hw} to determine the platform that we're building
# for, and store its name in ${platform}.

uname := ${shell uname | sed 's/_NT-.*//' | tr '[:upper:]' '[:lower:]'}

# Linux
ifeq (${uname},linux)
    platform := linux
    ifeq (${hw},x86_64)
        platform := linux64
    endif
endif

# Windows
ifeq (${uname},cygwin)
    platform := win
    ifeq (${hw},x86_64)
        platform := win64
    endif
endif

# Mac OS X
ifeq (${uname},darwin)
    platform := osx
endif

# If we haven't been able to determine the platform from uname, use
# whatever is in $ARCH, if it's set.
ifeq (${platform},unknown)
    ifneq (${ARCH},)
	platform := ${ARCH}
    endif
endif

# Manual override: if there's an environment variable $BUILDARCH, use that
# no matter what
ifneq (${BUILDARCH},)
    platform := ${BUILDARCH}
endif

# Throw an error if nothing worked
ifeq (${platform},unknown)
    $(error "ERROR: Could not determine the platform")
endif

$(info platform=${platform}, hw=${hw})

# end of section where we figure out the platform
#########################################################################



#########################################################################
# Default macros used by "most" platforms, so the platform-specific
# makefiles can be minimal

BINEXT :=
OEXT := .o
LIBPREFIX := lib
LIBEXT := .a
SHLIBEXT := .so

AR := ar cr
AROUT :=
ARPREREQ = $?
CFLAGS := -I${src_include_dir}
LDFLAGS := -rdynamic
LD_LIBPATH := -L
SHLIB_LDFLAGS := -Bdynamic -rdynamic -shared ${PIC} 
restrict_syms := -Wl,--version-script=${restrict_syms_file}
DASHC := -c #
DASHO := -o #
SHLIB_DASHO := -o #
BINOUT := -o #

MAKEDEPEND := makedepend
DEPENDFLAGS :=
DEPENDARGS :=

RM := rm
RM_ALL := rm -rf
CHMOD_WRITE := chmod +w
CHMOD_NOWRITE := chmod -w
CHMOD_RX := chmod 555
STRIP_BINARY := strip
MKDIR := mkdir -p
CP := cp -uvp
SED := sed
LD := ${CXX}
LDSHLIB := ${CXX}
# ld?


#
#########################################################################


#########################################################################
#

#
#########################################################################


# Include the platform-specific rules
include ${src_make_dir}/${platform}.mk




