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


#include "imageio.h"
#include "sysutil.h"
#include "argparse.h"
#include "strutil.h"
#include "timer.h"
#include "unittest.h"

#include <iostream>

OIIO_NAMESPACE_USING;




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
        int n = 1000000;
        for (int i = 0;  i < n;  ++i) {
            Timer t;
            t();  // force getting the time
        }
        std::cout << "Timer begin/end cost is " 
                  << double(n)/timer() << " /sec\n";
    }

    const int interval = 100000;  // 1/10 sec
    const double eps = 1e-2;   // slop we allow in our timings

    // Verify that Timer(false) doesn't start
    Timer all(true);
    Timer selective(false);
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

    return unit_test_failures;
}
