/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////


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
    ok = texsys->gettextureinfo (filename, ustring("resolution"),
                                 ParamType(PT_INT,2), res);
    std::cerr << "Result of gettextureinfo resolution = " << ok << ' ' << res[0] << 'x' << res[1] << "\n";

    int chan;
    ok = texsys->gettextureinfo (filename, ustring("channels"),
                                 PT_INT, &chan);
    std::cerr << "Result of gettextureinfo channels = " << ok << ' ' << chan << "\n";

    float fchan;
    ok = texsys->gettextureinfo (filename, ustring("channels"),
                                 PT_FLOAT, &fchan);
    std::cerr << "Result of gettextureinfo channels = " << ok << ' ' << fchan << "\n";

    const char *datetime = NULL;
    ok = texsys->gettextureinfo (filename, ustring("DateTime"),
                                 PT_STRING, &datetime);
    std::cerr << "Result of gettextureinfo datetime = " << ok << ' ' 
              << (datetime ? datetime : "") << "\n";

    const char *texturetype = NULL;
    ok = texsys->gettextureinfo (filename, ustring("textureformat"),
                                 PT_STRING, &texturetype);
    std::cerr << "Texture type is " << ok << ' '
              << (texturetype ? texturetype : "") << "\n";
    std::cerr << "\n";
}



static void
test_plain_texture (ustring filename)
{
    std::cerr << "Testing 2d texture " << filename << ", output = " 
              << output_filename << "\n";
    const int nchannels = 4;
    const int shadepoints = 32;
    ImageIOFormatSpec outspec (output_xres, output_yres, nchannels, PT_HALF);
    ImageBuf image (output_filename, outspec);
    image.zero ();

    Imath::M33f scale;  scale.scale (Imath::V2f (0.5, 0.5));
    Imath::M33f rot;    rot.rotate (radians(30.0f));
    Imath::M33f trans;  trans.translate (Imath::V2f (0.35f, 0.15f));
    Imath::M33f xform = scale * rot * trans;
    xform.invert();

    TextureOptions opt;
    opt.nchannels = 3;
    float s[shadepoints], t[shadepoints];
    Runflag runflags[shadepoints] = { RunFlagOn };

    for (int y = 0;  y < output_yres;  ++y) {
        for (int x = 0;  x < output_xres;  ++x) {
            Imath::V3f coord ((float)x/output_xres, (float)y/output_yres, 1.0f);
            Imath::V3f xcoord;
//            xform.multVecMatrix (coord, xcoord);
            coord *= xform;
            float s = coord[0], t = coord[1];
            float val[nchannels] = { 0, 0, 0, 1 };
            texsys->texture (filename, opt, runflags, 0, 0, s, t,
                             NULL, NULL, NULL, NULL, val);
            image.setpixel (x, y, val);
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



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    texsys = TextureSystem::create ();
    std::cerr << "Created texture system\n";

    ustring filename (filenames[0]);
    test_gettextureinfo (filename);

    const char *texturetype = NULL;
    bool ok = texsys->gettextureinfo (filename, ustring("texturetype"),
                                      PT_STRING, &texturetype);
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

    delete texsys;
    return 0;
}
