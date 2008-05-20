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
static int tile[3] = { 0, 0, 1 };
static bool scanline = false;



static void
usage (void)
{
    std::cout << 
        "Usage:  iconvert [options] infile outfile\n"
        "    --help                      Print this help message\n"
        "    -v [--verbose]              Verbose status messages\n"
        "    -d %s [--data-format %s]    Set the output format to one of:\n"
        "                                   uint8, sint8, uint16, sint16, half, float\n"
//        "    -f %s [--format %s]         FIXME Set the output format to one of:\n"
//        "                                   uint8, sint8, uint16, sint16, half, float\n"
        "    -g %f [--gamma %f]          Set gamma correction (default=1)\n"
        "    --tile %d %d [%d]           Output as a tiled image (2D or 3D)\n"
        "    --scanline                  Output as a scanline image\n"
        "    -z                          FIXME Treat input as a depth file\n"
        "    -c %s [--channels %s]       FIXME Restrict/shuffle channels\n"
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
        else if (! strcmp (argv[i], "-v") || ! strcmp (argv[i], "--verbose")) {
            verbose = true;
            continue;
        }
        else if (! strcmp (argv[i], "-g") || ! strcmp (argv[i], "--gamma")) {
            if (i < argc-1) {
                gammaval = atof (argv[++i]);
                if (gammaval > 0)
                    continue;
            }
            std::cerr << "iconvert: -g argument needs to be followed by a positive gamma value\n";
            usage();
            exit (0);
        }
        else if (! strcmp (argv[i], "-d") || ! strcmp (argv[i], "--data-format")) {
            if (i < argc-1) {
                dataformatname = argv[++i];
                continue;
            } else {
                std::cerr << "iconvert: -f argument needs to be followed by a string argument\n";
                usage();
                exit (0);
            }
        }
        else if (! strcmp (argv[i], "--scanline")) {
            scanline = true;
            continue;
        }
        else if (! strcmp (argv[i], "--tile")) {
            bool err = false;
            if (i < argc-2) {
                int t0 = atoi (argv[++i]);
                int t1 = atoi (argv[++i]);
                if (t0 > 0 && t1 > 0) {
                    tile[0] = t0;
                    tile[1] = t1;
                    int t2;
                    if (i < argc-1 && (t2 = atoi(argv[i+1])) > 0) {
                        tile[2] = t2;
                        ++i;
                    }
                } else err = true;
            } else {
                err = true;
            }
            if (err) {
                std::cerr << "iconvert: --tile argument needs at least 2 size values\n";
                usage();
                exit (0);
            }
        }
        else {
            filenames.push_back (argv[i]);
        }
    }
    if (filenames.size() != 2) {
        std::cerr << "iconvert: Must have both an input and output filename specified.\n";
        usage();
        exit (0);
    }
    std::cerr << "Converting " << filenames[0] << " to " << filenames[1] << "\n";
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    // Find an ImageIO plugin that can open the input file, and open it.
    ImageInput *in = ImageInput::create (filenames[0].c_str(), "" /* searchpath */);
    if (! in) {
        std::cerr 
            << "iconvert ERROR: Could not find an ImageIO plugin to read \"" 
            << filenames[0] << "\" : " << OpenImageIO::error_message() << "\n";;
        exit (0);
    }
    ImageIOFormatSpec inspec;
    if (! in->open (filenames[0].c_str(), inspec)) {
        std::cerr << "iconvert ERROR: Could not open \"" << filenames[0]
                  << "\" : " << in->error_message() << "\n";
        delete in;
        exit (0);
    }

    // Copy the spec, with possible change in format
    ImageIOFormatSpec outspec = inspec;
    outspec.set_format (inspec.format);
    if (! dataformatname.empty()) {
        if (dataformatname == "uint8")
            outspec.set_format (PT_UINT8);
        else if (dataformatname == "int8")
            outspec.set_format (PT_INT8);
        else if (dataformatname == "uint16")
            outspec.set_format (PT_UINT16);
        else if (dataformatname == "int16")
            outspec.set_format (PT_INT16);
        else if (dataformatname == "half")
            outspec.set_format (PT_HALF);
        else if (dataformatname == "float")
            outspec.set_format (PT_FLOAT);
        else if (dataformatname == "double")
            outspec.set_format (PT_DOUBLE);
    }
    outspec.gamma = gammaval;

    if (tile[0]) {
        outspec.tile_width = tile[0];
        outspec.tile_height = tile[1];
        outspec.tile_depth = tile[2];
    }
    if (scanline) {
        outspec.tile_width = 0;
        outspec.tile_height = 0;
        outspec.tile_depth = 0;
    }

    // Find an ImageIO plugin that can open the output file, and open it
    ImageOutput *out = ImageOutput::create (filenames[1].c_str());
    if (! out) {
        std::cerr 
            << "iconvert ERROR: Could not find an ImageIO plugin to write \"" 
            << filenames[1] << "\" :" << OpenImageIO::error_message() << "\n";;
        exit (0);
    }
    if (! out->open (filenames[1].c_str(), outspec)) {
        std::cerr << "iconvert ERROR: Could not open \"" << filenames[1]
                  << "\" : " << in->error_message() << "\n";
        delete in;
        exit (0);
    }

    char *pixels = new char [outspec.image_bytes()];
    in->read_image (outspec.format, pixels);
    in->close ();
    delete in;
    in = NULL;
    out->write_image (outspec.format, pixels);
    out->close ();
    delete out;
    delete [] pixels;

    return 0;
}
