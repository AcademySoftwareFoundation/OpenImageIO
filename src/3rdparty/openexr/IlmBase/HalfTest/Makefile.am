## Process this file with automake to produce Makefile.in

check_PROGRAMS = HalfTest

HalfTest_SOURCES = main.cpp testArithmetic.cpp testArithmetic.h \
		   testBitPatterns.cpp testBitPatterns.h \
		   testClassification.cpp testClassification.h \
		   testError.cpp testError.h testFunction.cpp \
		   testFunction.h testLimits.cpp testLimits.h testSize.cpp \
		   testSize.h

INCLUDES = -I$(top_builddir) -I$(top_srcdir)/Half -I$(top_srcdir)/config

LDADD = -L$(top_builddir)/Half -lHalf

TESTS = HalfTest

EXTRA_DIST = CMakeLists.txt

