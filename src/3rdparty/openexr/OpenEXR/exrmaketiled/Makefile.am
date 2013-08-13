## Process this file with automake to produce Makefile.in

bin_PROGRAMS = exrmaketiled

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/IlmImf -I$(top_srcdir)/config \
	   @ILMBASE_CXXFLAGS@

LDADD = @ILMBASE_LDFLAGS@ @ILMBASE_LIBS@ \
	$(top_builddir)/IlmImf/libIlmImf.la \
	-lz

exrmaketiled_SOURCES = main.cpp \
		       Image.h Image.cpp \
		       makeTiled.cpp makeTiled.h \
		       namespaceAlias.h

noinst_HEADERS = Image.h makeTiled.h

EXTRA_DIST = main.cpp \
	     Image.h Image.cpp \
	     makeTiled.cpp makeTiled.h \
	     namespaceAlias.h \
             CMakeLists.txt
