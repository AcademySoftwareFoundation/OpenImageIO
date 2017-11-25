/*
  Copyright 2012 Larry Gritz and the other authors and contributors.
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


#include <OpenImageIO/imageio.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/unittest.h>

#include <iostream>

using namespace OIIO;




static int
parse_files (int argc, const char *argv[])
{
//    input_filename = ustring(argv[0]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("timer_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  timer_test [options]",
                "%*", parse_files, "",
                "--help", &help, "Print help message",
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
main (int argc, char **argv)
{
    getargs (argc, argv);

    // First, just compute and print how expensive a Timer begin/end is,
    // in cycles per second.
    {
        Timer timer;
        int n = 10000000;
        int zeroes = 0;
        Timer::ticks_t biggest = 0;
        for (int i = 0;  i < n;  ++i) {
            Timer t;
            Timer::ticks_t ticks = t.ticks();  // force getting the time
            if (!ticks)
                ++zeroes;
            if (ticks > biggest)
                biggest = ticks;
        }
        std::cout << "Timer begin/end cost is " 
                  << double(n)/timer() << " /sec\n";
        std::cout << "Out of " << n << " queries, " << zeroes << " had no time\n";
        std::cout << "Longest was " << Timer::seconds(biggest) << " s.\n";
    }

    const int interval = 100000;  // 1/10 sec
    double eps = 0.01;   // slop we allow in our timings
#ifdef __APPLE__
    eps = 0.03;
    // On some Apple OSX systems (especially >= 10.10 Yosemite), a feature
    // called "timer coalescing" causes sleep/wake events to merge in order
    // to produce longer idle periods for the CPU to go into a lower power
    // state. This tends to make usleep() less reliable in its timing.
    //
    // One (permanent) fix is to disable timer coalescing with this command:
    //     $ sudo sysctl -w kern.timer.coalescing_enabled=0
    // But you want better power use, so instead we just increase the timing
    // tolereance on Apple to make this test pass.
# if defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // It seems especially bad on Travis, give extra time slop.
    // Got even worse in Nov 2017 on Travis. Make the slop enormous.
    eps = 0.5;
# endif
#endif

    // Verify that Timer(false) doesn't start
    Timer all (Timer::StartNow);
    Timer selective (Timer::DontStartNow);
    Sysutil::usleep (interval);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.0, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.1, eps);

    // Make sure start/stop work
    selective.start ();
    Sysutil::usleep (interval);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.1, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.2, eps);
    selective.stop ();
    Sysutil::usleep (interval);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.1, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.3, eps);
    std::cout << "Checkpoint: All " << all() << " selective " << selective() << "\n";

    // Test reset() -- should set selective to 0 and turn it off
    selective.reset();
    Sysutil::usleep (interval);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.0, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.4, eps);
    selective.start();
    Sysutil::usleep (interval);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.1, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.5, eps);

    // Test lap()
    double lap = selective.lap();  // lap=.1, select.time_since_start
    OIIO_CHECK_EQUAL_THRESH (lap,         0.1, eps);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.1, eps);
    OIIO_CHECK_EQUAL_THRESH (selective.time_since_start(), 0.0, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.5, eps);
    Sysutil::usleep (interval);
    OIIO_CHECK_EQUAL_THRESH (selective(), 0.2, eps);
    OIIO_CHECK_EQUAL_THRESH (selective.time_since_start(), 0.1, eps);
    OIIO_CHECK_EQUAL_THRESH (all(),       0.6, eps);
    std::cout << "Checkpoint2: All " << all() << " selective " << selective() << "\n";


    // Test Benchmarker
    Benchmarker bench;
    bench ("string ctr", [&](){
        std::string x ("foo");
    });
    bench ("usleep(1000)", [&](){
        Sysutil::usleep(1000);
    });

    float val = 0.5;  clobber(val);
    simd::float4 val4 = val;  clobber(val4);

    bench ("add", [&](){ DoNotOptimize(val+1.5); });
    bench ("cos", [&](){ DoNotOptimize(std::cos(val)); });
    bench ("acos", [&](){ DoNotOptimize(std::acos(val)); });
    bench ("fast_acos", [&](){ DoNotOptimize(OIIO::fast_acos(val)); });

    bench ("sqrt", [&](){ DoNotOptimize(std::sqrt(val)); });
    bench.work (4);
    bench ("simd sqrt", [&](){ DoNotOptimize(sqrt(val4)); });
    return unit_test_failures;
}
