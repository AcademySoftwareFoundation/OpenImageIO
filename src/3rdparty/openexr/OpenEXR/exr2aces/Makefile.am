## Process this file with automake to produce Makefile.in

bin_PROGRAMS = exr2aces

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/IlmImf -I$(top_srcdir)/config \
	   @ILMBASE_CXXFLAGS@

LDADD = @ILMBASE_LDFLAGS@ @ILMBASE_LIBS@ \
	$(top_builddir)/IlmImf/libIlmImf.la \
	-lz

exr2aces_SOURCES = main.cpp

EXTRA_DIST = main.cpp CMakeLists.txt


