/*
  Copyright 2016 Larry Gritz and the other authors and contributors.
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

#include <vector>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filter.h>

using namespace OIIO;

static int iterations = 10;
static int ntrials = 5;
static bool verbose = false;
static bool normalize = false;
static int graphxres = 1280, graphyres = 500;
static int graphyzero = graphyres*3/4;
static int graphxzero = graphxres/2;
static float graphunit = 200;



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("fmath_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  fmath_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                // "--threads %d", &numthreads,
                //     ustring::format("Number of threads (default: %d)", numthreads).c_str(),
                "--iterations %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
                "--normalize", &normalize, "Normalize/rescale all filters to peak at 1",
                NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}




int
main (int argc, char *argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs (argc, argv);

    Benchmarker bench;
    bench.iterations (iterations);
    bench.trials (ntrials);
    // bench.units (Benchmarker::Unit::ms);

    ImageBuf graph (ImageSpec (graphxres, graphyres, 3, TypeDesc::UINT8));
    float white[3] = { 1, 1, 1 };
    float black[3] = { 0, 0, 0 };
    ImageBufAlgo::fill (graph, white);
    ImageBufAlgo::render_line (graph, 0, graphyzero, graphxres-1, graphyzero,
                               black);
    ImageBufAlgo::render_line (graph, graphxzero, 0, graphxzero, graphyres-1,
                               black);
    int lastx = 0, lasty = 0;
    for (int i = 0, e = Filter1D::num_filters(); i < e; ++i) {
        FilterDesc filtdesc;
        Filter1D::get_filterdesc (i, &filtdesc);
        Filter1D *f = Filter1D::create (filtdesc.name, filtdesc.width);
        float scale = normalize ? 1.0f/(*f)(0.0f) : 1.0f;
        // Graph it
        float color[3] = { 0.25f * (i&3), 0.25f * ((i>>2)&3), 0.25f * ((i>>4)&3) };
        ImageBufAlgo::render_text (graph, 10, 20+i*20, filtdesc.name,
                                   16, "" /*font name*/, color);
        for (int x = 0; x < graphxres; ++x) {
            float xx = float(x-graphxzero) / graphunit;
            float yy = (*f)(xx) * scale;
            int y = int (graphyzero - yy * graphunit);
            if (x > 0)
                ImageBufAlgo::render_line (graph, lastx, lasty, x, y, color);
            lastx = x; lasty = y;
        }

        // Time it
        const size_t ncalls = 100000;
        bench.work (ncalls);
        float ninv = (filtdesc.width/2.0f) / ncalls;
        bench (filtdesc.name, [=](){
            for (size_t i = 0; i < ncalls; ++i)
                DoNotOptimize ((*f)(i*ninv));
        });

        Filter1D::destroy (f);
    }

    graph.write ("filters.tif");

    return unit_test_failures != 0;
}
