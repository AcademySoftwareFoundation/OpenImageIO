## Process this file with automake to produce Makefile.in

# tell autoconf to include the m4 macros in the /m4 directory 
# (an alternative to the acinclude.m4 mechanism)
ACLOCAL_AMFLAGS = -I m4

PYIMATH_SUBDIRS = config PyIex PyImath PyIexTest  PyImathTest
PYIMATHNUMPY_SUBDIRS = PyImathNumpy

if BUILD_PYIMATHNUMPY
    MAYBE_PYIMATHNUMPY_SUBDIRS = $(PYIMATHNUMPY_SUBDIRS)
endif

SUBDIRS = $(PYIMATH_SUBDIRS) $(MAYBE_PYIMATHNUMPY_SUBDIRS)

DIST_SUBDIRS = \
	$(PYIMATH_SUBDIRS) \
	$(PYIMATHNUMPY_SUBDIRS) \
	#vc

EXTRA_DIST = \
	AUTHORS COPYING ChangeLog INSTALL LICENSE NEWS \
        README README.OSX bootstrap \
	pyilmbase.m4 \
	config.windows/PyIlmBaseConfig.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = PyIlmBase.pc

m4datadir = $(datadir)/aclocal
m4data_DATA = pyilmbase.m4

