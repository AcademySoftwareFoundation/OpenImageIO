## Process this file with automake to produce Makefile.in

pyexec_LTLIBRARIES = imathnumpymodule.la

imathnumpymodule_la_SOURCES = imathnumpymodule.cpp
imathnumpymodule_la_LDFLAGS = -avoid-version -module
imathnumpymodule_la_LIBADD  = $(top_builddir)/PyImath/libPyImath.la @BOOST_PYTHON_LIBS@

noinst_HEADERS = 

INCLUDES = @ILMBASE_CXXFLAGS@      \
           @NUMPY_CXXFLAGS@        \
           -I$(top_srcdir)/PyImath \
	       -I$(top_builddir)       \
	       -I$(top_srcdir)/config
