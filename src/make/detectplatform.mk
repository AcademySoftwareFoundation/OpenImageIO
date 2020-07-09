# License and copyright goes here


#########################################################################
# detectplatform.mk
#
# Figure out which platform we are building on/for, set ${platform} and
# ${hw}.
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
        ifneq (${hw},aarch64)
          $(error "ERROR: Unknown hardware architecture")
        endif
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
    ifeq (${hw},aarch64)
      platform := aarch64
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

ifneq (${VERBOSE},)
$(info platform=${platform}, hw=${hw})
endif

# end of section where we figure out the platform
#########################################################################

