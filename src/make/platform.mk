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
platform ?= unknown

# Use 'uname -m' to determine the hardware architecture.  This should
# return "x86" or "x86_64"
hw := ${shell uname -m}
#$(info hardware = ${hw})
ifneq (${hw},x86)
  ifneq (${hw},x86_64)
    ifneq (${hw},i386)
      ifneq (${hw},i686)
        $(error "ERROR: Unknown hardware architecture")
      endif
    endif
  endif
endif

# Use 'uname', lowercased and stripped of pesky stuff, and the hardware
# architecture in ${hw} to determine the platform that we're building
# for, and store its name in ${platform}.

uname := ${shell uname | sed 's/_NT-.*//' | tr '[:upper:]' '[:lower:]'}
#$(info uname = ${uname})
ifeq (${platform},unknown)
  # Linux
  ifeq (${uname},linux)
    platform := linux
    ifeq (${hw},x86_64)
      platform := linux64
    endif
  endif

  # Windows
  ifeq (${uname},cygwin)
    platform := windows
    ifeq (${hw},x86_64)
      platform := windows64
    endif
  endif

  # Mac OS X
  ifeq (${uname},darwin)
    platform := macosx
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

# C and C++ compilation
CFLAGS += -I${src_include_dir}
DASHC := -c #
DASHO := -o #
CINCL := -I
OEXT := .o

# Creating static libraries
LIBPREFIX := lib
LIBEXT := .a
AR := ar cr
AROUT :=
ARPREREQ = $?

# Linking an executable
BINEXT :=
LD := ${CXX}
BINOUT := -o #
LD_LIBPATH := -L
LDFLAGS += -rdynamic
LINKWITH := -l
#restrict_syms := -Wl,--version-script=${restrict_syms_file}

# Creating a dynamic/shared library
SHLIBEXT := .so
LDSHLIB := ${CXX}
SHLIB_DASHO := -o #
SHLIB_LDFLAGS += -Bdynamic -rdynamic -shared ${PIC} 

# Making dependency make files (.d)
MAKEDEPEND := makedepend
DEPENDFLAGS :=
DEPENDARGS :=

# Miscellaneous shell commands
RM := rm
RM_ALL := rm -rf
CHMOD_W := chmod +w
CHMOD_RO := chmod -w
CHMOD_RX := chmod 555
STRIP_BINARY := strip
MKDIR := mkdir -p
CP := cp -vpf
CPR := cp -vpfr
SED := sed
# ld?

QT_MOC ?= moc
FLEX := flex
BISON := bison

#
#########################################################################


#########################################################################
#

#
#########################################################################


# Include the platform-specific rules
include ${src_make_dir}/${platform}.mk

