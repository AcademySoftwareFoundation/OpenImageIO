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

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "imageio.h"
using namespace OpenImageIO;


static std::string dataformatname = "";
static float gammaval = 1.0f;
static bool depth = false;
static bool verbose = false;
static std::vector<std::string> filenames;



static void
usage (void)
{
    std::cout << 
        "Usage:  iconvert [options] infile outfile\n"
        "    --help                      Print this help message\n"
        "    -d %s [--data-format %s]    Set the output format to one of:\n"
        "                                   uint8, sint8, uint16, sint16, half, float\n"
        "    -f %s [--format %s]         Set the output format to one of:\n"
        "                                   uint8, sint8, uint16, sint16, half, float\n"
        "    -v [--verbose]              Verbose status messages\n"
        "    -g %f [--gamma %f]          Set gamma correction (default=1)\n"
        "    -z                          Treat input as a depth file\n"
        "    -c %s [--channels %s]       Restrict/shuffle channels\n"
        ;
}



static void
getargs (int argc, char *argv[])
{
#if 0
    float gamma = 1;
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "print help message")
            ("verbose,v", "verbose status messages")
            ("data-format,d", po::value<std::string>(), 
                 "set output data format to one of:\n"
                 "  uint8, sint8, uint16, sint16, half, float")
            ("gamma,g", po::value<float>(&gamma)->default_value(1),
                 "apply gamma correction to output")
            ("channels,c", po::value<std::string>(), "channel list")
            ("z,z", "treat input as a depth file")
            ("input-file", po::value< vector<std::string> >(), "input file")
        ;

        po::positional_options_description p;
        p.add("input-file", -1);
        po::variables_map vm;        
        po::store (po::command_line_parser(argc, argv).
                   options(desc).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "Usage:  iconvert [options] infile output\n";
            std::cout << desc << "\n";
            return;
        }

        if (vm.count ("data-format")) {
            string f = vm["data-format"].as<string>();
            std::cout << "Format = '" << f << "'\n";
        }
//        if (vm.count ("input-file")) {
//            vector<string> inputs = vm["input-file"].as< vector<string> >();
//            for (size_t i = 0;  i < inputs.size(); ++i)
//                std::cout << "input " << inputs[i] << '\n';
//        }
//        cout << "Inputs = " << vm["input-file"].as< vector<string> >() << "\n";
    }
    catch(exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n";
    }
#endif
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
        if (! strcmp (argv[i], "-g") || ! strcmp (argv[i], "--gamma")) {
            if (i < argc-1) {
                gammaval = atof (argv[++i]);
                continue;
            } else {
                std::cerr << "iconvert: -g argument needs to be followed by a numeric argument\n";
                usage();
                exit (0);
            }
        }
        if (! strcmp (argv[i], "-d") || ! strcmp (argv[i], "--data-format")) {
            if (i < argc-1) {
                dataformatname = argv[++i];
                continue;
            } else {
                std::cerr << "iconvert: -f argument needs to be followed by a string argument\n";
                usage();
                exit (0);
            }
        }
    }
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    const int res = 100;
    unsigned char *pixels = new unsigned char [res*res*3];
    for (int y = 0;  y < res;  ++y)
        for (int x = 0;  x < res;  ++x) {
            pixels[3*(y*res+x)+0] = x;
            pixels[3*(y*res+x)+1] = y;
            pixels[3*(y*res+x)+2] = 0;
        }

    ImageIOFormatSpec spec (PT_UINT8);
    spec.width = res;
    spec.height = res;
    spec.nchannels = 3;
    const char *compress = "deflate";
    spec.add_parameter ("compression", PT_STRING, 1, &compress);

    const char *filename = "out.tif";
    ImageOutput *out = ImageOutput::create (filename);
    if (!out) {
        std::cerr << "Could not create ImageOutput for " << filename <<  "\n";
        std::cerr << "  Error was: " << OpenImageIO::error_message() << "\n";
        exit (1);
    }
    std::cerr << "Checkpoint 1\n";
    out->open (filename, spec);
    std::cerr << "Checkpoint 2\n";
    for (int y = 0;  y < res;  ++y)
        out->write_scanline (y, 0, PT_UINT8, pixels+3*res*y, 3);
    std::cerr << "Checkpoint 3\n";
    out->close ();
    std::cerr << "Checkpoint 4\n";
    delete out;
    std::cerr << "Checkpoint 5\n";

    delete [] pixels;
    std::cerr << "Checkpoint 6\n";

    return 0;
}
