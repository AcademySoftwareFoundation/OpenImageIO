/*
  Copyright 2017 Larry Gritz and the other authors and contributors.
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

#include <cstdio>
#include <cstdlib>
#include <numeric>

#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/thread.h>


OIIO_NAMESPACE_BEGIN

namespace pvt {
void OIIO_API
#if __has_attribute(__optnone__)
__attribute__((__optnone__))
#endif
use_char_ptr (char const volatile *) {}
}  // end namespace


// Implementation of clobber_ptr is trivial, but the code in other modules
// doesn't know that.
void OIIO_API
#if __has_attribute(__optnone__)
__attribute__((__optnone__))
#endif
clobber (void* p) {}



double
Benchmarker::iteration_overhead ()
{
    static bool initialized = false;
    static double overhead = 0.0;
    if (! initialized) {
        auto trivial = [](){ };
        const size_t trials = 10;
        const size_t overhead_iterations = 10000000;
        std::vector<double> times (trials);
        for (auto& t : times)
            t = do_trial (overhead_iterations, trivial);
        compute_stats (times, overhead_iterations);
        overhead = median();
        initialized = true;
        // std::cout << "iteration overhead is " << overhead << "\n";
    }
    return overhead;
}



void
Benchmarker::compute_stats (std::vector<double>& times, size_t iterations)
{
    size_t trials = times.size();
    ASSERT (trials >= 1);
#if 0
    // Debugging: print all the trial times
    for (auto v : times)
        std::cout << v/iterations*1e6 << ' ';
    std::cout << "\n";
#endif

    // Sort so that we can exclude outliers
    std::sort (times.begin(), times.end());

    size_t first = 0, last = trials;
    if (size_t(2*exclude_outliers()+3) <= trials) {
        first += exclude_outliers();
        last  -= exclude_outliers();
    }
    size_t nt = last-first;
    if (nt == 1) {
        m_avg = times[first];
        m_stddev = 0;
        m_range = 0;
    } else {
        m_avg = std::accumulate (times.begin()+first, times.begin()+last, 0.0) / nt;
        double sum2 = std::accumulate (times.begin()+first, times.begin()+last, 0.0,
                                       [&](double a, double b) { return a + (b-m_avg)*(b-m_avg); });
        m_stddev = sqrt (sum2 / (nt-1));
        m_range = times[last-1] - times[first];
    }

    if (m_trials & 1) // odd
        m_median = times[m_trials/2];
    else
        m_median = 0.5 * (times[m_trials/2] +
                          times[m_trials/2+1]);

    m_avg /= iterations;
    m_stddev /= iterations;
    m_range /= iterations;
    m_median /= iterations;
}



OIIO_API
std::ostream& operator<< (std::ostream& out, const Benchmarker &bench)
{
    // Get local copies of relevant statistics
    double avg = bench.avg();
    double stddev = bench.stddev();
    double range = bench.range();

    // Figure out appropriate scale
    static const char* unitnames[] = { "ns", "ns", "us", "ms", "s" };
    static double unitscales[] = { 1e9, 1e9, 1e6, 1e3, 1 };
    int unit = int(bench.units());
    if (unit == int(Benchmarker::Unit::autounit)) {
        while (unit < int(Benchmarker::Unit::s) && bench.avg()*unitscales[unit] > 10000.0)
            ++unit;
    }
    const char* unitname = unitnames[unit];
    double scale = unitscales[unit];
    char rateunit = 'M';
    double ratescale = 1.0e6;
    if (bench.avg() >= 1.0e-6) {
        rateunit = 'k';
        ratescale = 1.0e3;
    }

    avg *= scale;
    stddev *= scale;
    range *= scale;

    if (bench.indent())
        out << std::string (bench.indent(), ' ');
    if (unit == int(Benchmarker::Unit::s))
        out << Strutil::format ("%-16s: %s", bench.m_name,
                                Strutil::timeintervalformat(avg, 2));
    else
        out << Strutil::format ("%-16s: %6.1f %s (+/-%4.1f%s), ",
                                bench.name(), avg, unitname, stddev, unitname);
    if (bench.avg() < 0.25e-9) {
        // Less than 1/4 ns iteration time is probably an error
        out << "unreliable";
        return out;
    }
    if (bench.work() == 1)
        out << Strutil::format ("%6.1f %c/s",
                                (1.0f/ratescale)/bench.avg(), rateunit);
    else
        out << Strutil::format ("%6.1f %cvals/s, %.1f %ccalls/s",
                                (bench.work()/ratescale)/bench.avg(), rateunit,
                                (1.0f/ratescale)/bench.avg(), rateunit);
    if (bench.verbose() >= 2)
        out << Strutil::format (" (%dx%d, rng=%.1f%%, med=%.1f)",
                                bench.trials(), bench.iterations(), unitname,
                                (range/avg)*100.0, bench.median()*scale);
#if 0
    if (range > avg/10.0) {
        for (auto v : bench.m_times)
            std::cout << v*scale/bench.iterations() << ' ';
        std::cout << "\n";
    }
#endif
    return out;
}



OIIO_API std::vector<double>
timed_thread_wedge (function_view<void(int)> task,
                    function_view<void()> pretask,
                    function_view<void()> posttask,
                    std::ostream *out,
                    int maxthreads,
                    int total_iterations, int ntrials,
                    array_view<const int> threadcounts)
{
    std::vector<double> times (threadcounts.size(), 0.0f);
    if (out)
        (*out) << "threads    time   speedup  efficient  its/thread   range (best of " << ntrials << ")\n";
    for (size_t i = 0; i < threadcounts.size(); ++i) {
        int nthreads = threadcounts[i];
        if (nthreads > maxthreads)
            continue;
        int iters = total_iterations/nthreads;
        double range;
        times[i]= time_trial ([&](){
                                  pretask();
                                  thread_group threads;
                                   for (int t = 0; t < nthreads; ++t)
                                       threads.create_thread (task, iters);
                                   threads.join_all();
                                   posttask();
                               },
                               ntrials, &range);
        if (out) {
            double one_thread_time = times[0] * threadcounts[0];
            double ideal = one_thread_time / nthreads;
            double speedup = one_thread_time / times[i];
            double efficiency = 100.0 * ideal / times[i];
            (*out) << Strutil::format ("%4d   %8.1f   %6.2fx    %6.2f%% %10d %8.2f\n",
                                       nthreads, times[i], speedup,
                                       efficiency, iters, range);
            out->flush();
        }
    }
    return times;
}



OIIO_API void
timed_thread_wedge (function_view<void(int)> task,
                    int maxthreads, int total_iterations, int ntrials,
                    array_view<const int> threadcounts)
{
    timed_thread_wedge (task, [](){}, [](){}, &std::cout,
                        maxthreads, total_iterations, ntrials, threadcounts);
}

OIIO_NAMESPACE_END
