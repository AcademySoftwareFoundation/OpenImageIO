/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
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
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


/// Simple timer class


#ifndef TIMER_H
#define TIMER_H

#ifdef _WIN32
#else
#include <sys/time.h>
#endif


class Timer {
public:
#ifdef _WIN32
    typedef ??? value_t;
#else
    typedef struct timeval value_t;
#endif

    /// Constructor -- reset at zero, and start timing unless optional
    /// 'startnow' argument is false.
    Timer (bool startnow=true) : m_ticking(false), m_elapsed(0)
    {
        if (startnow)
            start();
    }

    /// Destructor.
    ///
    ~Timer () { }

    /// Start ticking, or restart if we have stopped.
    ///
    void start () {
        if (! m_ticking) {
            m_starttime = now();
            m_ticking = true;
        }
    }

    /// Stop ticking.  Any elapsed time will be saved even though we
    /// aren't currently ticking.
    void stop () {
        if (m_ticking) {
            value_t n = now();
            m_elapsed += diff (n, m_starttime);
        }
        m_ticking = false;
    }

    /// Reset at zero and stop ticking.
    ///
    void reset (void) {
        m_elapsed = 0;
        m_ticking = false;
    }

    /// Operator () returns the elapsed time so far, including both the
    /// currently-ticking clock as well as any previously elapsed time.
    double operator() (void) const {
        return m_elapsed + time_since_start();
    }

    /// Return just the time since we called start(), not any elapsed
    /// time in previous start-stop segments.
    double time_since_start (void) const {
        if (m_ticking) {
            value_t n = now();
            return diff (m_starttime, n);
        } else {
            return 0;
        }
    }

private:
    bool m_ticking;       ///< Are we currently ticking?
    value_t m_starttime;  ///< Time since last call to start()
    double m_elapsed;     ///< Time elapsed BEFORE the current start().

    /// Platform-dependent grab of current time, expressed as value_t.
    ///
    value_t now (void) const {
#ifdef _WIN32
#else
        value_t n;
        gettimeofday (&n, NULL);
        return n;
#endif
    }

    /// Platform-dependent difference between two times, expressed in
    /// seconds.
    static double diff (const value_t &then, const value_t &now) {
#ifdef _WIN32
#else
        return fabs ((now.tv_sec  - then.tv_sec) +
                     (now.tv_usec - then.tv_usec) / 1e6);
#endif
    }
};



/// Helper class that starts and stops a timer when the ScopedTimer goes
/// in and out of scope.
template <class TIMER=Timer>
class ScopedTimer {
public:
    ScopedTimer (TIMER &t) : m_timer(t) { start(); }
    ~ScopedTimer () { stop(); }
    void start () { m_timer.start(); }
    void stop () { m_timer.stop(); }
    void reset () { m_timer.reset(); }
private:
    TIMER &m_timer;
};


#endif // TIMER_H

