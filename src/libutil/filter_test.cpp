// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebufalgo_util.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;

static int iterations = 10;
static int ntrials    = 5;
static bool verbose   = false;
static bool normalize = false;
static int graphxres = 1280, graphyres = 500;
static int graphyzero  = graphyres * 3 / 4;
static int graphxzero  = graphxres / 2;
static float graphunit = 200;



static void
getargs(int argc, char* argv[])
{
    bool help = false;
    ArgParse ap;
    // clang-format off
    ap.options(
        "filter_test\n" OIIO_INTRO_STRING "\n"
        "Usage:  filter_test [options]",
        // "%*", parse_files, "",
        "--help", &help, "Print help message",
        "-v", &verbose, "Verbose mode",
        // "--threads %d", &numthreads,
        //     ustring::sprintf("Number of threads (default: %d)", numthreads).c_str(),
        "--iterations %d", &iterations,
            ustring::sprintf("Number of iterations (default: %d)", iterations).c_str(),
        "--trials %d", &ntrials, "Number of trials",
        "--normalize", &normalize, "Normalize/rescale all filters to peak at 1",
        nullptr);
    // clang-format on
    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        exit(EXIT_FAILURE);
    }
    if (help) {
        ap.usage();
        exit(EXIT_FAILURE);
    }
}



int
main(int argc, char* argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    Benchmarker bench;
    bench.iterations(iterations);
    bench.trials(ntrials);
    // bench.units (Benchmarker::Unit::ms);

    ImageBuf graph(ImageSpec(graphxres, graphyres, 3, TypeDesc::UINT8));
    float white[3] = { 1, 1, 1 };
    float black[3] = { 0, 0, 0 };
    ImageBufAlgo::fill(graph, white);
    ImageBufAlgo::render_line(graph, 0, graphyzero, graphxres - 1, graphyzero,
                              black);
    ImageBufAlgo::render_line(graph, graphxzero, 0, graphxzero, graphyres - 1,
                              black);
    int lastx = 0, lasty = 0;
    for (int i = 0, e = Filter1D::num_filters(); i < e; ++i) {
        FilterDesc filtdesc;
        Filter1D::get_filterdesc(i, &filtdesc);
        Filter1D* f = Filter1D::create(filtdesc.name, filtdesc.width);
        float scale = normalize ? 1.0f / (*f)(0.0f) : 1.0f;
        // Graph it
        float color[3] = { 0.25f * (i & 3), 0.25f * ((i >> 2) & 3),
                           0.25f * ((i >> 4) & 3) };
        ImageBufAlgo::render_text(graph, 10, 20 + i * 20, filtdesc.name, 16,
                                  "" /*font name*/, color);
        for (int x = 0; x < graphxres; ++x) {
            float xx = float(x - graphxzero) / graphunit;
            float yy = (*f)(xx)*scale;
            int y    = int(graphyzero - yy * graphunit);
            if (x > 0)
                ImageBufAlgo::render_line(graph, lastx, lasty, x, y, color);
            lastx = x;
            lasty = y;
        }

        // Time it
        const size_t ncalls = 100000;
        bench.work(ncalls);
        float ninv = (filtdesc.width / 2.0f) / ncalls;
        bench(filtdesc.name, [=]() {
            for (size_t i = 0; i < ncalls; ++i)
                DoNotOptimize((*f)(i * ninv));
        });

        Filter1D::destroy(f);
    }

    graph.write("filters.tif");

    return unit_test_failures != 0;
}
