## Process this file with automake to produce Makefile.in

check_PROGRAMS = IexTest

IexTest_SOURCES = main.cpp testBaseExc.cpp testBaseExc.h

INCLUDES = -I$(top_builddir) -I$(top_srcdir)/Iex -I$(top_srcdir)/config

LDADD = -L$(top_builddir)/Iex -lIex

TESTS = IexTest

EXTRA_DIST = CMakeLists.txt
