// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
    use_char_ptr(char const volatile*)
{
}

}  // namespace pvt


// Implementation of clobber_ptr is trivial, but the code in other modules
// doesn't know that.
void OIIO_API
#if __has_attribute(__optnone__)
    __attribute__((__optnone__))
#endif
    clobber(void*)
{
}



double
Benchmarker::iteration_overhead()
{
    static bool initialized = false;
    static double overhead  = 0.0;
    if (!initialized) {
        auto trivial                     = []() {};
        const size_t trials              = 10;
        const size_t overhead_iterations = 10000000;
        std::vector<double> times(trials);
        for (auto& t : times)
            t = do_trial(overhead_iterations, trivial);
        compute_stats(times, overhead_iterations);
        overhead    = median();
        initialized = true;
        // std::cout << "iteration overhead is " << overhead << "\n";
    }
    return overhead;
}



void
Benchmarker::compute_stats(std::vector<double>& times, size_t iterations)
{
    size_t trials = times.size();
    OIIO_ASSERT(trials >= 1);
#if 0
    // Debugging: print all the trial times
    for (auto v : times)
        std::cout << v/iterations*1e6 << ' ';
    std::cout << "\n";
#endif

    // Sort so that we can exclude outliers
    std::sort(times.begin(), times.end());

    size_t first = 0, last = trials;
    if (size_t(2 * exclude_outliers() + 3) <= trials) {
        first += exclude_outliers();
        last -= exclude_outliers();
    }
    size_t nt = last - first;
    if (nt == 1) {
        m_avg    = times[first];
        m_stddev = 0;
        m_range  = 0;
    } else {
        m_avg = std::accumulate(times.begin() + first, times.begin() + last,
                                0.0)
                / nt;
        double sum2 = std::accumulate(times.begin() + first,
                                      times.begin() + last, 0.0,
                                      [&](double a, double b) {
                                          return a + (b - m_avg) * (b - m_avg);
                                      });
        m_stddev    = sqrt(sum2 / (nt - 1));
        m_range     = times[last - 1] - times[first];
    }

    if (m_trials & 1)  // odd
        m_median = times[m_trials / 2];
    else
        m_median = 0.5 * (times[m_trials / 2] + times[m_trials / 2 + 1]);

    m_avg /= iterations;
    m_stddev /= iterations;
    m_range /= iterations;
    m_median /= iterations;
}



OIIO_API
std::ostream&
operator<<(std::ostream& out, const Benchmarker& bench)
{
    // Get local copies of relevant statistics
    double avg    = bench.avg();
    double stddev = bench.stddev();
    double range  = bench.range();

    // Figure out appropriate scale
    static const char* unitnames[] = { "ns", "ns", "us", "ms", "s" };
    static double unitscales[]     = { 1e9, 1e9, 1e6, 1e3, 1 };
    int unit                       = int(bench.units());
    if (unit == int(Benchmarker::Unit::autounit)) {
        while (unit < int(Benchmarker::Unit::s)
               && bench.avg() * unitscales[unit] > 10000.0)
            ++unit;
    }
    const char* unitname = unitnames[unit];
    double scale         = unitscales[unit];
    char rateunit        = 'M';
    double ratescale     = 1.0e6;
    if (bench.avg() >= 1.0e-6) {
        rateunit  = 'k';
        ratescale = 1.0e3;
    }

    avg *= scale;
    stddev *= scale;
    range *= scale;

    if (bench.indent())
        print(out, "{}", std::string(bench.indent(), ' '));
    if (unit == int(Benchmarker::Unit::s))
        print(out, "{:16}: {}", bench.m_name,
              Strutil::timeintervalformat(avg, 2));
    else
        print(out, "{:16}: {:6.1f} {} (+/- {:.1f}{}), ", bench.name(), avg,
              unitname, stddev, unitname);
    if (bench.avg() < 0.25e-9) {
        // Less than 1/4 ns iteration time is probably an error
        print(out, "unreliable");
        return out;
    }
    if (bench.work() == 1)
        print(out, "{:6.1f} {:c}/s", (1.0f / ratescale) / bench.avg(),
              rateunit);
    else
        print(out, "{:6.1f} {:c}vals/s, {:.1} {:c}calls/s",
              (bench.work() / ratescale) / bench.avg(), rateunit,
              (1.0f / ratescale) / bench.avg(), rateunit);
    if (bench.verbose() >= 2)
        print(out, " ({}x{}, rng={:.1}%, med={:.1})", bench.trials(),
              bench.iterations(), unitname, (range / avg) * 100.0,
              bench.median() * scale);
#if 0
    if (range > avg/10.0) {
        for (auto v : bench.m_times)
            print(out, "{} ", v*scale/bench.iterations());
        print(out, "\n");
    }
#endif
    return out;
}



OIIO_API std::vector<double>
timed_thread_wedge(function_view<void(int)> task, function_view<void()> pretask,
                   function_view<void()> posttask, std::ostream* out,
                   int maxthreads, int total_iterations, int ntrials,
                   cspan<int> threadcounts)
{
    std::vector<double> times(threadcounts.size(), 0.0f);
    if (out)
        Strutil::print(
            *out,
            "threads    time   speedup  efficient  its/thread   range (best of {})\n",
            ntrials);
    for (size_t i = 0; i < (size_t)threadcounts.size(); ++i) {
        int nthreads = threadcounts[i];
        if (nthreads > maxthreads)
            continue;
        int iters = total_iterations / nthreads;
        double range;
        times[i] = time_trial(
            [&]() {
                pretask();
                thread_group threads;
                for (int t = 0; t < nthreads; ++t)
                    threads.create_thread(task, iters);
                threads.join_all();
                posttask();
            },
            ntrials, 1, &range);
        if (out) {
            double one_thread_time = times[0] * threadcounts[0];
            double ideal           = one_thread_time / nthreads;
            double speedup         = one_thread_time / times[i];
            double efficiency      = 100.0 * ideal / times[i];
            Strutil::print(
                *out, "{:4}   {:8.1f}   {:6.2f}x    {:6.2f}% {:10} {:8.2f}\n",
                nthreads, times[i], speedup, efficiency, iters, range);
        }
    }
    return times;
}



OIIO_API void
timed_thread_wedge(function_view<void(int)> task, int maxthreads,
                   int total_iterations, int ntrials, cspan<int> threadcounts)
{
    timed_thread_wedge(
        task, []() {}, []() {}, &std::cout, maxthreads, total_iterations,
        ntrials, threadcounts);
}

OIIO_NAMESPACE_END
