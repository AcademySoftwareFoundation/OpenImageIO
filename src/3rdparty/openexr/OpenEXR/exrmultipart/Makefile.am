## Process this file with automake to produce Makefile.in

bin_PROGRAMS = exrmultipart

INCLUDES = -I$(top_builddir) \
-I$(top_srcdir)/IlmImf -I$(top_srcdir)/config \
@ILMBASE_CXXFLAGS@

LDADD = @ILMBASE_LDFLAGS@ @ILMBASE_LIBS@ \
$(top_builddir)/IlmImf/libIlmImf.la \
-lz

exrmultipart_SOURCES = exrmultipart.cpp

EXTRA_DIST = CMakeLists.txt

