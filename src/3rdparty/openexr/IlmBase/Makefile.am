## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	Half HalfTest \
	Iex IexMath IexTest \
	Imath ImathTest \
	IlmThread \
	config

DIST_SUBDIRS = \
	$(SUBDIRS) \
	vc


EXTRA_DIST = \
	AUTHORS COPYING ChangeLog INSTALL LICENSE NEWS \
	README README.CVS README.OSX README.win32 bootstrap \
	config.windows/IlmBaseConfig.h \
        CMakeLists.txt


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = IlmBase.pc

