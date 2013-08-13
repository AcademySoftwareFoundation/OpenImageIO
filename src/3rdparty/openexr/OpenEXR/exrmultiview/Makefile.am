## Process this file with automake to produce Makefile.in

bin_PROGRAMS = exrmultiview

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/IlmImf -I$(top_srcdir)/config \
	   @ILMBASE_CXXFLAGS@

LDADD = @ILMBASE_LDFLAGS@ @ILMBASE_LIBS@ \
	$(top_builddir)/IlmImf/libIlmImf.la \
	-lz

exrmultiview_SOURCES = main.cpp  \
		       Image.h Image.cpp \
		       makeMultiView.cpp makeMultiView.h \
		       namespaceAlias.h

noinst_HEADERS = Image.h makeMultiView.h

EXTRA_DIST = main.cpp \
	     Image.h Image.cpp \
	     makeMultiView.h makeMultiView.cpp \
	     namespaceAlias.h \
             CMakeLists.txt
