// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
    // clang-format off
    ArgParse ap;
    ap.intro("filter_test\n" OIIO_INTRO_STRING)
      .usage("filter_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    // ap.arg("--threads %d", &numthreads)
    //   .help(Strutil::sprintf("Number of threads (default: %d)", numthreads));
    ap.arg("--iters %d", &iterations)
      .help(Strutil::sprintf("Number of iterations (default: %d)", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    ap.arg("--normalize", &normalize)
      .help("Normalize/rescale all filters to peak at 1");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



void
test_1d()
{
    print("Testing 1D filters\n");

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
        // Graph it
        float scale          = normalize ? 1.0f / (*f)(0.0f) : 1.0f;
        float color[3]       = { 0.25f * (i & 3), 0.25f * ((i >> 2) & 3),
                                 0.25f * ((i >> 4) & 3) };
        std::string filtname = filtdesc.name;
        if (filtdesc.name != f->name())
            filtname = Strutil::fmt::format("{} ({})", filtname, f->name());
        ImageBufAlgo::render_text(graph, 10, 20 + i * 20, filtname, 16,
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
        bench(filtdesc.name, [=]() { DoNotOptimize((*f)(0.25f)); });

        Filter1D::destroy(f);
    }

    graph.write("filters.tif");
}



void
test_2d()
{
    print("\nTesting 2D filters\n");

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
    for (int i = 0, e = Filter2D::num_filters(); i < e; ++i) {
        FilterDesc filtdesc;
        Filter2D::get_filterdesc(i, &filtdesc);
        Filter2D* f = Filter2D::create(filtdesc.name, filtdesc.width,
                                       filtdesc.width);
        // Graph it
        float scale          = normalize ? 1.0f / (*f)(0.0f, 0.0f) : 1.0f;
        float color[3]       = { 0.25f * (i & 3), 0.25f * ((i >> 2) & 3),
                                 0.25f * ((i >> 4) & 3) };
        std::string filtname = filtdesc.name;
        if (filtdesc.name != f->name())
            filtname = Strutil::fmt::format("{} ({})", filtname, f->name());
        ImageBufAlgo::render_text(graph, 10, 20 + i * 20, filtname, 16,
                                  "" /*font name*/, color);
        for (int x = 0; x < graphxres; ++x) {
            float xx = float(x - graphxzero) / graphunit;
            float yy = (*f)(xx, 0.0f) * scale;
            int y    = int(graphyzero - yy * graphunit);
            if (x > 0)
                ImageBufAlgo::render_line(graph, lastx, lasty, x, y, color);
            lastx = x;
            lasty = y;
        }

        // Time it
        bench(filtdesc.name, [=]() { DoNotOptimize((*f)(0.25f, 0.25f)); });

        Filter2D::destroy(f);
    }

    graph.write("filters2d.tif");
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

    test_1d();
    test_2d();
}
