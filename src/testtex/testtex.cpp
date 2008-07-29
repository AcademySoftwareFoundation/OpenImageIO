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

#include "argparse.h"
#include "imageio.h"
using namespace OpenImageIO;
#include "ustring.h"
#include "texture.h"


static std::vector<std::string> filenames;
static bool verbose = false;

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
    if (ap.parse ("Usage:  testtex [options] inputfile outputfile",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  NULL) < 0) {
        std::cerr << ap.error_message() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

#if 0
    if (filenames.size() != 2) {
        std::cerr << "iconvert: Must have both an input and output filename specified.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
#endif
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    texsys = TextureSystem::create ();
    std::cerr << "Created texture system\n";

    bool ok;

    int res[2];
    ok = texsys->gettextureinfo (ustring("grid.tx"), ustring("resolution"),
                                 ParamType(PT_INT,2), res);
    std::cerr << "Result of gettextureinfo resolution = " << ok << "\n";

    delete texsys;
    return 0;
}
