## Process this file with automake to produce Makefile.in

if BUILD_IMFEXAMPLES
noinst_PROGRAMS = imfexamples
endif

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/IlmImf -I$(top_srcdir)/config \
	   @ILMBASE_CXXFLAGS@

LDADD = -L$(top_builddir)/IlmImf \
	@ILMBASE_LDFLAGS@ @ILMBASE_LIBS@ \
	-lIlmImf -lz

imfexamples_SOURCES = main.cpp drawImage.cpp rgbaInterfaceExamples.cpp \
		      rgbaInterfaceTiledExamples.cpp \
		      generalInterfaceExamples.cpp \
		      lowLevelIoExamples.cpp previewImageExamples.cpp \
		      generalInterfaceTiledExamples.cpp \
		      generalInterfaceTiledExamples.h drawImage.h \
		      rgbaInterfaceExamples.h generalInterfaceExamples.h \
		      rgbaInterfaceTiledExamples.h \
		      lowLevelIoExamples.h previewImageExamples.h \
		      namespaceAlias.h

examplesdir = $(datadir)/doc/OpenEXR-@OPENEXR_VERSION@/examples
examples_DATA = $(imfexamples_SOURCES)

imfexamplesdir = $(examplesdir)

EXTRA_DIST = CMakeLists.txt

