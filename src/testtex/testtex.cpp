/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iterator>

#include <ImathMatrix.h>
#include <ImathVec.h>

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;
#include "ustring.h"
#include "imagebuf.h"
#include "texture.h"
#include "fmath.h"


static std::vector<std::string> filenames;
static std::string output_filename = "out.exr";
static bool verbose = false;
static int output_xres = 512, output_yres = 512;
static float blur = 0;
static int iters = 1;
static TextureSystem *texsys = NULL;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap (argc, (const char **)argv);
    if (ap.parse ("Usage:  testtex [options] inputfile",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-o %s", &output_filename, "Output test image",
                  "-res %d %d", &output_xres, &output_yres,
                      "Resolution of output test image",
                  "-iters %d", &iters,
                      "Iterations for time trials",
                  "--blur %f", &blur, "Add blur to texture lookup",
                  NULL) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() < 1) {
        std::cerr << "testtex: Must have at least one input file\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
}



static void
test_gettextureinfo (ustring filename)
{
    bool ok;

    int res[2];
    ok = texsys->get_texture_info (filename, ustring("resolution"),
                                   TypeDesc(TypeDesc::INT,2), res);
    std::cerr << "Result of get_texture_info resolution = " << ok << ' ' << res[0] << 'x' << res[1] << "\n";

    int chan;
    ok = texsys->get_texture_info (filename, ustring("channels"),
                                   TypeDesc::INT, &chan);
    std::cerr << "Result of get_texture_info channels = " << ok << ' ' << chan << "\n";

    float fchan;
    ok = texsys->get_texture_info (filename, ustring("channels"),
                                   TypeDesc::FLOAT, &fchan);
    std::cerr << "Result of get_texture_info channels = " << ok << ' ' << fchan << "\n";

    const char *datetime = NULL;
    ok = texsys->get_texture_info (filename, ustring("DateTime"),
                                   TypeDesc::STRING, &datetime);
    std::cerr << "Result of get_texture_info datetime = " << ok << ' ' 
              << (datetime ? datetime : "") << "\n";

    const char *texturetype = NULL;
    ok = texsys->get_texture_info (filename, ustring("textureformat"),
                                   TypeDesc::STRING, &texturetype);
    std::cerr << "Texture type is " << ok << ' '
              << (texturetype ? texturetype : "") << "\n";
    std::cerr << "\n";
}


Imath::V3f
warp (float x, float y, Imath::M33f &xform)
{
    Imath::V3f coord (x, y, 1.0f);
    coord *= xform;
    coord[0] *= 1/(1+2*std::max (-0.5f, coord[1]));
    return coord;
}


static void
test_plain_texture (ustring filename)
{
    std::cerr << "Testing 2d texture " << filename << ", output = " 
              << output_filename << "\n";
    const int nchannels = 4;
    const int shadepoints = 32;
    ImageSpec outspec (output_xres, output_yres, nchannels, TypeDesc::HALF);
    ImageBuf image (output_filename, outspec);
    image.zero ();

    Imath::M33f scale;  scale.scale (Imath::V2f (0.5, 0.5));
    Imath::M33f rot;    rot.rotate (radians(30.0f));
    Imath::M33f trans;  trans.translate (Imath::V2f (0.35f, 0.15f));
    Imath::M33f xform = scale * rot * trans;
    xform.invert();

    TextureOptions opt;
    opt.sblur = blur;
    opt.tblur = blur;
    opt.nchannels = 3;
//    opt.interpmode = TextureOptions::InterpSmartBicubic;
//    opt.mipmode = TextureOptions::MipModeAniso;
    opt.swrap = opt.twrap = TextureOptions::WrapPeriodic;
//    opt.twrap = TextureOptions::WrapBlack;
    float s[shadepoints], t[shadepoints];
    Runflag runflags[shadepoints] = { RunFlagOn };

    for (int iter = 0;  iter < iters;  ++iter) {
        for (int y = 0;  y < output_yres;  ++y) {
            for (int x = 0;  x < output_xres;  ++x) {
                Imath::V3f coord = warp ((float)x/output_xres, (float)y/output_yres, xform);
                Imath::V3f coordx = warp ((float)(x+1)/output_xres, (float)y/output_yres, xform);
                Imath::V3f coordy = warp ((float)x/output_xres, (float)(y+1)/output_yres, xform);
                float s = coord[0], t = coord[1];
                float dsdx = coordx[0] - s;
                float dtdx = coordx[1] - t;
                float dsdy = coordy[0] - s;
                float dtdy = coordy[1] - t;
                float val[nchannels] = { 0, 0, 0, 1 };
                texsys->texture (filename, opt, runflags, 0, 0, s, t,
                                 dsdx, dtdx, dsdy, dtdy, val);
                image.setpixel (x, y, val);
            }
        }
    }
    
    if (! image.save ()) 
        std::cerr << "Error writing " << output_filename 
                  << " : " << image.error_message() << "\n";
}



static void
test_shadow (ustring filename)
{
}



static void
test_environment (ustring filename)
{
}



static void
test_getimagespec_gettexels (ustring filename)
{
    ImageSpec spec;
    if (! texsys->get_imagespec (filename, spec)) {
        std::cerr << "Could not get spec for " << filename << "\n";
        return;
    }
    int w = spec.width/2, h = spec.height/2;
    ImageSpec postagespec (w, h, spec.nchannels, TypeDesc::FLOAT);
    ImageBuf buf ("postage.exr", postagespec);
    TextureOptions opt;
    opt.nchannels = spec.nchannels;
    texsys->get_texels (filename, opt, 0, w/2, w/2+w-1, h/2, h/2+h-1, 0, 0, 
                        postagespec.format, buf.pixeladdr (0,0));
    buf.save ();
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    texsys = TextureSystem::create ();
    std::cerr << "Created texture system\n";

    ustring filename (filenames[0]);
    test_gettextureinfo (filename);

    const char *texturetype = NULL;
    bool ok = texsys->get_texture_info (filename, ustring("texturetype"),
                                        TypeDesc::STRING, &texturetype);
    if (ok) {
        if (! strcmp (texturetype, "Plain Texture")) {
            test_plain_texture (filename);
        }
        if (! strcmp (texturetype, "Shadow")) {
            test_shadow (filename);
        }
        if (! strcmp (texturetype, "Environment")) {
            test_environment (filename);
        }
    }
    test_getimagespec_gettexels (filename);

    
    TextureSystem::destroy (texsys);
    return 0;
}
