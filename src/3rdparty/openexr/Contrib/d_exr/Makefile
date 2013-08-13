#
# Makefile for OpenEXR display driver for Pixar's PhotoRealistic RenderMan
# 3D rendering system.  Running 'make' with no arguments builds the driver
# and runs the renderer on a test RIB file with the display driver using
# each of the supported compression schemes plus uncompressed.
# Set RMANTREE variable to the base of the RenderMan installation.
# Change the include and lib paths for your OpenEXR installation as
# appropriate.  They're set to /usr/local/{lib,include} by default, which
# is the default install directory for OpenEXR.
#
# Note:
# Pixar offers two releases of PRMAN for Linux: a base version and
# an optimized version.  The base version is currently (12Feb2003)
# built with an older release of GCC that won't compile the OpenEXR 
# libraries.  The optimized PRMAN version is built with gcc 2.96,
# which compiles OpenEXR just fine.  To use this display driver, then,
# compile OpenEXR with gcc 2.96 and use the optimized PRMAN.
# See Pixar's RenderMan documentation for instructions on installing and 
# using the optimized version.
#

CXX = g++

DEFINES = -Drestrict=__restrict__
INCLUDES = -I/usr/local/include/OpenEXR -I$(RMANTREE)/include
LIBRARIES = -L/usr/local/lib -lIlmImf -lImath -lIex -lHalf -lz

all: test

d_exr.so: d_exr.o
	$(CXX) -o d_exr.so -shared d_exr.o $(LIBRARIES)

.C.o:
	$(CXX) -c $(DEFINES) $(INCLUDES) $<

test: d_exr.so
	sed 's/EXRCOMP/none/g' tst.rib | $(RMANTREE)/bin/render
	sed 's/EXRCOMP/rle/g' tst.rib | $(RMANTREE)/bin/render
	sed 's/EXRCOMP/zips/g' tst.rib | $(RMANTREE)/bin/render
	sed 's/EXRCOMP/zip/g' tst.rib | $(RMANTREE)/bin/render
	sed 's/EXRCOMP/piz/g' tst.rib | $(RMANTREE)/bin/render
	sed 's/EXRCOMP/piz12/g' tst.rib | $(RMANTREE)/bin/render

clean:
	rm -f d_exr.o

nuke: clean
	rm -f d_exr.so tst.*.exr core
