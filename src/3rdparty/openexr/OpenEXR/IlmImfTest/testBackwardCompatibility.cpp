///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#include "testBackwardCompatibility.h"

#include <ImfArray.h>
#include <ImfHeader.h>
#include <IlmThreadPool.h>
#include <ImfFrameBuffer.h>
#include <ImfOutputFile.h>
#include <ImfPreviewImage.h>
#include <ImfTiledOutputFile.h>
#include <stdlib.h>
#include <stdio.h>
#include <tmpDir.h>
#include <ImathBox.h>
#include <ImfChannelList.h>
#include <IexMacros.h>

#include <ImfBoxAttribute.h>
#include <ImfChannelListAttribute.h>
#include <ImfCompressionAttribute.h>
#include <ImfChromaticitiesAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfEnvmapAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfLineOrderAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfOpaqueAttribute.h>
#include <ImfStringAttribute.h>
#include <ImfStringVectorAttribute.h>
#include <ImfVecAttribute.h>

#include <half.h>

#include <iostream>
#include <fstream>
#include <string>
#include <assert.h>
#include <time.h>
#ifndef WIN32
#include <sys/times.h>
#endif // WIN32

#ifndef ILM_IMF_TEST_IMAGEDIR
    #define ILM_IMF_TEST_IMAGEDIR
#endif


using namespace OPENEXR_IMF_NAMESPACE;
using namespace std;
using namespace IMATH_NAMESPACE;
using namespace ILMTHREAD_NAMESPACE;

namespace {

const int generateImagesOnly = false;


const int W = 217;
const int H = 197;

const char * planarScanlineName      = IMF_TMP_DIR "v1.7.test.planar.exr";
const char * interleavedScanlineName = IMF_TMP_DIR "v1.7.test.interleaved.exr";
const char * tiledName               = IMF_TMP_DIR "v1.7.test.tiled.exr";


void
diffImageFiles (const char * fn1, const char * fn2)
{
    ifstream i1 (fn1, ios::binary);
    ifstream i2 (fn2, ios::binary);

    if(!i1.good()){THROW (IEX_NAMESPACE::BaseExc, string("cannot open ") + string(fn1));}
    if(!i2.good()){THROW (IEX_NAMESPACE::BaseExc, string("cannot open ") + string(fn2));}

    while (!i1.eof() && !i2.eof())
    {
        if (i1.get() != i2.get())
        {
            string e = string ("v1.7 and current differences between '") +
                       string (fn1) + string ("' & '") +  string (fn2) +
                       string ("'");
            THROW (IEX_NAMESPACE::BaseExc, e);
        }
    }
}

void
addPreviewImageToHeader (OPENEXR_IMF_NAMESPACE::Header & hdr)
{
    size_t pW = 32;
    size_t pH = 32;

    OPENEXR_IMF_NAMESPACE::Array2D <OPENEXR_IMF_NAMESPACE::PreviewRgba> previewPixels (pW, pH);
    for (size_t h=0; h<pH; h++)
    {
        for (size_t w=0; w<pW; w++)
        {
            previewPixels[w][h] = (w*h) % 255;
        }
    }
    hdr.setPreviewImage (OPENEXR_IMF_NAMESPACE::PreviewImage (pW, pH, &previewPixels[0][0]));
}

void
addUserAttributesToHeader (OPENEXR_IMF_NAMESPACE::Header & hdr)
{
    Box2i  a1  (V2i (1, 2), V2i (3, 4));
    Box2f  a2  (V2f (1.5, 2.5), V2f (3.5, 4.5));
    float  a3  (3.14159);
    int    a4  (17);
    M33f   a5  (11, 12, 13, 14, 15, 16, 17, 18, 19);
    M44f   a6  (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    string a7  ("extensive rebuilding by Nebuchadrezzar has left");
    V2i    a8  (27, 28);
    V2f    a9  (27.5, 28.5);
    V3i    a10 (37, 38, 39);
    V3f    a11 (37.5, 38.5, 39.5);
    double a12 (7.12342341419);
    Chromaticities a13 (V2f (1, 2), V2f (3, 4), V2f (5, 6), V2f (7, 8));
//    Envmap a14 (ENVMAP_CUBE);
    StringVector a15;
    a15.push_back ("who can spin");
    a15.push_back ("");
    a15.push_back ("straw into");
    a15.push_back ("gold");

    M33d   a16 (12, 13, 14, 15, 16, 17, 18, 19, 20);
    M44d   a17 (2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
    V2d    a18 (27.51, 28.51);
    V3d    a19 (37.51, 38.51, 39.51);

    hdr.insert ("a1",  Box2iAttribute  (a1));
    hdr.insert ("a2",  Box2fAttribute  (a2));
    hdr.insert ("a3",  FloatAttribute  (a3));
    hdr.insert ("a4",  IntAttribute    (a4));
    hdr.insert ("a5",  M33fAttribute   (a5));
    hdr.insert ("a6",  M44fAttribute   (a6));
    hdr.insert ("a7",  StringAttribute (a7));
    hdr.insert ("a8",  V2iAttribute    (a8));
    hdr.insert ("a9",  V2fAttribute    (a9));
    hdr.insert ("a10", V3iAttribute    (a10));
    hdr.insert ("a11", V3fAttribute    (a11));
    hdr.insert ("a12", DoubleAttribute (a12));
    hdr.insert ("a13", ChromaticitiesAttribute (a13));
//    hdr.insert ("a14", EnvmapAttribute         (a14));
    hdr.insert ("a15", StringVectorAttribute   (a15));
    hdr.insert ("a16", M33dAttribute   (a16));
    hdr.insert ("a17", M44dAttribute   (a17));
    hdr.insert ("a18", V2dAttribute    (a18));
    hdr.insert ("a19", V3dAttribute    (a19));

}

void
generateScanlinePlanarImage (const char * fn)
{
    // generate a v 1.7 image and check against ground truth on disk
    Array2D<float> pf (H, W);  pf.resizeErase(H, W);
    Array2D<half>  ph (H, W);  ph.resizeErase(H, W);

    for (int i = 0; i < H; i++)
    {
        for (int j = 0; j < W; j++)
        {
            pf[i][j] = (float)((i * W + j) /  (float(W*H)));
            ph[i][j] = (half)(pf[i][j]);
        }
    }

    IMATH_NAMESPACE::Box2i dod (IMATH_NAMESPACE::V2f(20), IMATH_NAMESPACE::V2f(W-20, H-23));
    OPENEXR_IMF_NAMESPACE::Header header = Header (W, H, dod);
    header.channels().insert("Z", Channel(FLOAT));
    header.channels().insert("R", Channel(HALF));
    header.channels().insert("G", Channel(HALF));
    header.channels().insert("B", Channel(HALF));
    addUserAttributesToHeader (header);

    FrameBuffer fb;

    fb.insert ("Z",
               Slice (FLOAT,
                      (char *) &pf[0][0],
                      sizeof (pf[0][0]),
                      sizeof (pf[0][0]) * W));

    fb.insert ("R",
               Slice (HALF,
                      (char *) &ph[0][0],
                      sizeof (ph[0][0]),
                      sizeof (ph[0][0]) * W));
    fb.insert ("G",
               Slice (HALF,
                      (char *) &ph[0][0],
                      sizeof (ph[0][0]),
                      sizeof (ph[0][0]) * W));
    fb.insert ("B",
               Slice (HALF,
                      (char *) &ph[0][0],
                      sizeof (ph[0][0]),
                      sizeof (ph[0][0]) * W));

    OutputFile file (fn, header);
    file.setFrameBuffer (fb);
    file.writePixels (H-40);
}

struct RZ
{
    float z;
    half g;
};

void
generateScanlineInterleavedImage (const char * fn)
{
    // generate a v 1.7 image and check against ground truth on disk
    Array2D<RZ> rz (H, W);  rz.resizeErase(H, W);

    for (int i = 0; i < H; i++)
    {
        for (int j = 0; j < W; j++)
        {
            rz[i][j].z = (float)((i * W + j) /  (float(W*H)));
            rz[i][j].g = (half)(rz[i][j].z);
        }
    }

    IMATH_NAMESPACE::Box2i dod (IMATH_NAMESPACE::V2f(20), IMATH_NAMESPACE::V2f(W-20, H-23));
    OPENEXR_IMF_NAMESPACE::Header header = Header (W, H, dod);
    header.channels().insert("Z", Channel(FLOAT));
    header.channels().insert("R", Channel(HALF));
    addUserAttributesToHeader (header);

    FrameBuffer fb;

    fb.insert ("Z",
               Slice (FLOAT,
                      (char *) &(rz[0][0].z),
                      sizeof (rz[0][0]),
                      sizeof (rz[0][0]) * W));

    fb.insert ("G",
               Slice (HALF,
                      (char *) &(rz[0][0].g),
                      sizeof (rz[0][0]),
                      sizeof (rz[0][0]) * W));

    OutputFile file (fn, header);
    file.setFrameBuffer (fb);
    file.writePixels (H-40);
}

void
diffScanlineImages ()
{
    // Planar Images
    generateScanlinePlanarImage (planarScanlineName);
    diffImageFiles (planarScanlineName, ILM_IMF_TEST_IMAGEDIR "v1.7.test.planar.exr");
    remove(planarScanlineName);

    // Interleaved Images
    generateScanlineInterleavedImage (interleavedScanlineName);
    diffImageFiles (interleavedScanlineName, ILM_IMF_TEST_IMAGEDIR "v1.7.test.interleaved.exr");
    remove(interleavedScanlineName);
}


void
generateTiledImage (const char * fn)
{
    Array2D<RZ> rz (H, W);  rz.resizeErase(H, W);

    for (int i = 0; i < H; i++)
    {
        for (int j = 0; j < W; j++)
        {
            rz[i][j].z = (float)((i * W + j) /  (float(W*H)));
            rz[i][j].g = (half)(rz[i][j].z);
        }
    }

    Header header (W, H);
    header.channels().insert ("G", Channel (HALF));
    header.channels().insert ("Z", Channel (FLOAT));

    int tileW = 12;
    int tileH = 24;
    header.setTileDescription (TileDescription (tileW, tileH, ONE_LEVEL));

    OPENEXR_IMF_NAMESPACE::TiledOutputFile out (fn, header);
    OPENEXR_IMF_NAMESPACE::FrameBuffer frameBuffer; // 6
    frameBuffer.insert ("G",
                        Slice (HALF,
                               (char *) &rz[0][0].g,
                               sizeof (rz[0][0]) * 1,
                               sizeof (rz[0][0]) * W));

    frameBuffer.insert ("Z",
                        Slice (FLOAT,
                               (char *) &rz[0][0].z,
                               sizeof (rz[0][0]) * 1,
                               sizeof (rz[0][0]) * W));

    out.setFrameBuffer (frameBuffer);
    out.writeTiles (0, out.numXTiles() - 1, 0, out.numYTiles() - 1);
}


void
diffTiledImages ()
{
    // Planar Images
    generateTiledImage (tiledName);
    diffImageFiles (tiledName, ILM_IMF_TEST_IMAGEDIR "v1.7.test.tiled.exr");
    remove(tiledName);
}


} // namespace


void
testBackwardCompatibility ()
{
    try
    {
        cout << "Testing backward compatibility" << endl;

        if (generateImagesOnly)
        {
            generateScanlinePlanarImage ("v1.7.test.planar.exr");
            generateScanlineInterleavedImage ("v1.7.test.interleaved.exr");
            generateTiledImage ("v1.7.test.tiled.exr");
        }
        else
        {
            diffScanlineImages ();
            diffTiledImages ();
        }

        cout << "ok\n" << endl;
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR -- caught exception: " << e.what() << endl;
        assert (false);
    }
}
