## Process this file with automake to produce Makefile.in

check_PROGRAMS = IlmImfTest

IlmImfTest_SOURCES = main.cpp tmpDir.h testAttributes.cpp testChannels.cpp \
		     testCompression.cpp testCopyPixels.cpp \
		     testCustomAttributes.cpp testHuf.cpp testLineOrder.cpp \
		     testLut.cpp testRgba.cpp testRgbaThreading.cpp \
		     testSampleImages.cpp testSharedFrameBuffer.cpp \
		     testWav.cpp testXdr.cpp testAttributes.h testChannels.h \
		     testCompression.h testCopyPixels.h \
		     testCustomAttributes.h testHuf.h testLineOrder.h \
		     testLut.h testRgba.h testRgbaThreading.h \
		     testSampleImages.h testWav.h testSharedFrameBuffer.h \
		     testXdr.h testConversion.cpp testConversion.h \
		     testNativeFormat.cpp testNativeFormat.h \
		     testPreviewImage.cpp testPreviewImage.h \
		     testMagic.cpp testMagic.h testStandardAttributes.cpp \
		     testStandardAttributes.h testExistingStreams.cpp \
		     testExistingStreams.h testScanLineApi.cpp \
		     testScanLineApi.h testTiledCompression.cpp \
		     testTiledCompression.h testTiledCopyPixels.cpp \
		     testTiledCopyPixels.h testTiledLineOrder.cpp \
		     testTiledLineOrder.h testTiledRgba.cpp \
		     testTiledRgba.h compareFloat.h compareFloat.cpp \
		     testTiledYa.cpp testTiledYa.h \
		     testYca.cpp testYca.h compareB44.h compareB44.cpp \
		     testMultiView.cpp testMultiView.h \
		     testIsComplete.cpp testIsComplete.h \
		     testMultiPartApi.cpp testMultiPartApi.h \
		     testMultiPartThreading.cpp testMultiPartThreading.h \
		     testMultiScanlinePartThreading.cpp testMultiScanlinePartThreading.h \
		     testMultiTiledPartThreading.cpp testMultiTiledPartThreading.h \
		     testDeepScanLineBasic.cpp testDeepScanLineBasic.h \
		     testDeepTiledBasic.cpp testDeepTiledBasic.h \
		     testMultiPartFileMixingBasic.cpp testMultiPartFileMixingBasic.h \
		     testMultiPartSharedAttributes.cpp testMultiPartSharedAttributes.h \
		     testBackwardCompatibility.cpp testBackwardCompatibility.h \
		     testCopyDeepScanLine.cpp testCopyDeepScanLine.h \
		     testCopyDeepTiled.cpp testCopyDeepTiled.h \
		     testCopyMultiPartFile.h testCopyMultiPartFile.cpp \
		     testCompositeDeepScanLine.h testCompositeDeepScanLine.cpp \
		     testInputPart.cpp testInputPart.h \
		     testDeepScanLineMultipleRead.h testDeepScanLineMultipleRead.cpp  \
		     testPartHelper.h testPartHelper.cpp \
		     testOptimized.cpp testOptimized.h \
		     testOptimizedInterleavePatterns.cpp testOptimizedInterleavePatterns.h \
		     testBadTypeAttributes.cpp testBadTypeAttributes.h \
		     testFutureProofing.cpp testFutureProofing.h

AM_CPPFLAGS = -DILM_IMF_TEST_IMAGEDIR=\"$(srcdir)/\"

if BUILD_IMFHUGETEST
IlmImfTest_SOURCES += testDeepScanLineHuge.cpp testDeepScanLineHuge.h
AM_CPPFLAGS += -DENABLE_IMFHUGETEST
endif


INCLUDES = -I$(top_builddir)  \
	   -I$(top_srcdir)/IlmImf \
	   -I$(top_srcdir)/config \
	   @ILMBASE_CXXFLAGS@

LDADD = -L$(top_builddir)/IlmImf \
	@ILMBASE_LDFLAGS@ @ILMBASE_LIBS@ \
	-lIlmImf -lz

TESTS = IlmImfTest

EXTRA_DIST = comp_none.exr comp_piz.exr comp_rle.exr comp_zip.exr \
	     comp_zips.exr lineOrder_decreasing.exr lineOrder_increasing.exr \
	     test_native1.exr test_native2.exr invalid.exr \
	     tiled.exr comp_b44.exr comp_b44_piz.exr \
             v1.7.test.planar.exr  v1.7.test.tiled.exr v1.7.test.1.exr v1.7.test.interleaved.exr \
             invalid_shared_attrs_multipart.exr \
	     tiled_with_scanlineimage_type.exr scanline_with_tiledimage_type.exr \
	     tiled_with_deepscanline_type.exr tiled_with_deeptile_type.exr \
	     scanline_with_deepscanline_type.exr scanline_with_deeptiled_type.exr \
             CMakeLists.txt
