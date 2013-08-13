## Process this file with automake to produce Makefile.in

bin_PROGRAMS = exrmakepreview

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/IlmImf -I$(top_srcdir)/config \
	   @ILMBASE_CXXFLAGS@

LDADD = @ILMBASE_LDFLAGS@ @ILMBASE_LIBS@\
	$(top_builddir)/IlmImf/libIlmImf.la \
	-lz

exrmakepreview_SOURCES = main.cpp makePreview.cpp makePreview.h

noinst_HEADERS = makePreview.h

EXTRA_DIST = main.cpp makePreview.cpp makePreview.h CMakeLists.txt
