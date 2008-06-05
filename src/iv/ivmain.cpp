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

#include <QtGui>

#include <boost/foreach.hpp>

#include "imageio.h"
using namespace OpenImageIO;
#include "imageviewer.h"
#include "timer.h"


static bool verbose = false;
static bool sum = false;
static std::vector<std::string> filenames;



static void
usage (void)
{
    std::cout << 
        "Usage:  iv [options] filename...\n"
        "    --help                      Print this help message\n"
        "    -v [--verbose]              Verbose output\n"
        "    -s                          Sum the image sizes\n";
        ;
}



static void
getargs (int argc, char *argv[])
{
    for (int i = 1;  i < argc;  ++i) {
        std::cerr << "arg " << i << " : " << argv[i] << '\n';
        if (! strcmp (argv[i], "-h") || ! strcmp (argv[i], "--help")) {
            usage();
            exit (0);
        }
        if (! strcmp (argv[i], "-v") || ! strcmp (argv[i], "--verbose")) {
            verbose = true;
            continue;
        }
        if (! strcmp (argv[i], "-s")) {
            sum = true;
            continue;
        }
        filenames.push_back (argv[i]);
    }
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    // LG
//    Q_INIT_RESOURCE(iv);
    QApplication app(argc, argv);
    ImageViewer *mainWin = new ImageViewer;
    mainWin->show();

    BOOST_FOREACH (const std::string &s, filenames) {
        mainWin->add_image (s);
    }

    int r = app.exec();
    // OK to clean up here
    return r;
}
