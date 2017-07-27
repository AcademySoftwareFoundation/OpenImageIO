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


#ifndef OPENIMAGEIO_PARALLEL_H
#define OPENIMAGEIO_PARALLEL_H
#pragma once

#include <algorithm>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <OpenImageIO/thread.h>


OIIO_NAMESPACE_BEGIN


/// Parallel "for" loop, chunked: for a task that takes an int thread ID
/// followed by an int64_t [begin,end) range, break it into non-overlapping
/// sections that run in parallel using the default thread pool:
///
///    task (threadid, start, start+chunksize);
///    task (threadid, start+chunksize, start+2*chunksize);
///    ...
///    task (threadid, start+n*chunksize, end);
///
/// and wait for them all to complete.
///
/// If chunksize is 0, a chunksize will be chosen to divide the range into
/// a number of chunks equal to the twice number of threads in the queue.
/// (We do this to offer better load balancing than if we used exactly the
/// thread count.)
/// 
/// Note that the thread_id may be -1, indicating that it's being executed
/// by the calling thread itself, or perhaps some other helpful thread that
/// is stealing work from the pool.
inline void
parallel_for_chunked (int64_t start, int64_t end, int64_t chunksize,
                   std::function<void(int id, int64_t b, int64_t e)>&& task)
{
    thread_pool *pool (default_thread_pool());
    if (chunksize < end-start && pool->this_thread_is_in_pool())
        chunksize = end-start;    // don't use the pool recursively
    if (chunksize < 1) {
        int p = std::max (1, 2*pool->size());
        chunksize = std::max (int64_t(1), (end-start) / p);
    }
    for (task_set<void> ts (pool); start < end; start += chunksize) {
        int64_t e = std::min (end, start+chunksize);
        if (e == end) {
            // For the last (or only) subtask, do it ourselves and avoid
            // messing with the queue or handing off between threads.
            task (-1, start, e);
        } else {
            ts.push (pool->push (task, start, e));
        }
    }
}


/// Parallel "for" loop, chunked: for a task that takes a [begin,end) range
/// (but not a thread ID).
inline void parallel_for_chunked (int64_t start, int64_t end, int64_t chunksize,
                           std::function<void(int64_t b, int64_t e)>&& task)
{
    thread_pool *pool (default_thread_pool());
    if (chunksize < end-start && pool->this_thread_is_in_pool())
        chunksize = end-start;    // don't use the pool recursively
    if (chunksize < 1) {
        int p = std::max (1, 2*pool->size());
        chunksize = std::max (int64_t(1), (end-start) / p);
    }
    auto wrapper = [&](int id, int64_t b, int64_t e){ task(b,e); };
    for (task_set<void> ts (pool); start < end; start += chunksize) {
        int64_t e = std::min (end, start+chunksize);
        if (e == end) {
            // For the last (or only) subtask, do it ourselves and avoid
            // messing with the queue or handing off between threads.
            task (start, e);
        } else {
            ts.push (pool->push (wrapper, start, std::min (end, e)));
        }
    }
}



/// Parallel "for" loop, for a task that takes a single int64_t index, run
/// it on all indices on the range [begin,end):
///
///    task (begin);
///    task (begin+1);
///    ...
///    task (end-1);
///
/// Using the default thread pool, spawn parallel jobs. Conceptually, it
/// behaves as if each index gets called separately, but actually each
/// thread will iterate over some chunk of adjacent indices (to aid data
/// coherence and minimuize the amount of thread queue diddling). The chunk
/// size is chosen automatically.
inline void
parallel_for (int64_t start, int64_t end,
              std::function<void(int64_t index)>&& task)
{
    parallel_for_chunked (start, end, 0, [&task](int id, int64_t i, int64_t e) {
        for ( ; i < e; ++i)
            task (i);
    });
}


/// parallel_for, for a task that takes an int threadid and an int64_t
/// index, running all of:
///    task (id, begin);
///    task (id, begin+1);
///    ...
///    task (id, end-1);
inline void
parallel_for (int64_t start, int64_t end,
              std::function<void(int id, int64_t index)>&& task)
{
    parallel_for_chunked (start, end, 0, [&task](int id, int64_t i, int64_t e) {
        for ( ; i < e; ++i)
            task (id, i);
    });
}



/// parallel_for_each, semantically is like std::for_each(), but each
/// iteration is a separate job for the default thread pool.
template<class InputIt, class UnaryFunction>
UnaryFunction
parallel_for_each (InputIt first, InputIt last, UnaryFunction f)
{
    thread_pool *pool (default_thread_pool());
    if (pool->size() == 1 || pool->this_thread_is_in_pool()) {
        // Don't use the pool recursively or if there are no workers --
        // just run the function directly.
        for ( ; first != last; ++first)
            f (*first);
    } else {
        for (task_set<void> ts (pool); first != last; ++first)
            ts.push (pool->push ([&](int id){ f(*first); }));
    }
    return std::move(f);
}



/// Parallel "for" loop in 2D, chunked: for a task that takes an int thread
/// ID followed by begin, end, chunksize for each of x and y, subdivide that
/// run in parallel using the default thread pool.
///
///    task (threadid, xstart, xstart+xchunksize, );
///    task (threadid, start+chunksize, start+2*chunksize);
///    ...
///    task (threadid, start+n*chunksize, end);
///
/// and wait for them all to complete.
///
/// If chunksize is 0, a chunksize will be chosen to divide the range into
/// a number of chunks equal to the twice number of threads in the queue.
/// (We do this to offer better load balancing than if we used exactly the
/// thread count.)
inline void
parallel_for_chunked_2D (int64_t xstart, int64_t xend, int64_t xchunksize,
                         int64_t ystart, int64_t yend, int64_t ychunksize,
                   std::function<void(int id, int64_t xbegin, int64_t xend,
                                      int64_t ybegin, int64_t yend)>&& task)
{
    thread_pool *pool (default_thread_pool());
    if (pool->this_thread_is_in_pool()) {
        // don't use the pool recursively
        task (-1, xstart, xend, ystart, yend);
        return;
    }
    if (ychunksize < 1)
        ychunksize = std::max (int64_t(1), (yend-ystart) / (pool->size()));
    if (xchunksize < 1) {
        int64_t ny = std::max (int64_t(1), (yend-ystart) / ychunksize);
        int64_t nx = std::max (int64_t(1), pool->size() / ny);
        xchunksize = std::max (int64_t(1), (xend-xstart) / nx);
    }
    task_set<void> ts (pool);
    for (auto y = ystart; y < yend; y += ychunksize) {
        int64_t ychunkend = std::min (yend, y+ychunksize);
        for (auto x = xstart; x < xend; x += xchunksize)
            ts.push (pool->push (task, x, std::min (xend, x+xchunksize),
                                 y, ychunkend));
    }
}



/// Parallel "for" loop, chunked: for a task that takes a 2D [begin,end)
/// range and chunk sizes.
inline void
parallel_for_chunked_2D (int64_t xstart, int64_t xend, int64_t xchunksize,
                         int64_t ystart, int64_t yend, int64_t ychunksize,
                     std::function<void(int64_t xbegin, int64_t xend,
                                        int64_t ybegin, int64_t yend)>&& task)
{
    auto wrapper = [&](int id, int64_t xb, int64_t xe,
                       int64_t yb, int64_t ye) { task(xb,xe,yb,ye); };
    parallel_for_chunked_2D (xstart, xend, xchunksize,
                             ystart, yend, ychunksize, wrapper);
}



/// parallel_for, for a task that takes an int threadid and int64_t x & y
/// indices, running all of:
///    task (id, xstart, ystart);
///    ...
///    task (id, xend-1, ystart);
///    task (id, xstart, ystart+1);
///    task (id, xend-1, ystart+1);
///    ...
///    task (id, xend-1, yend-1);
inline void
parallel_for_2D (int64_t xstart, int64_t xend,
                 int64_t ystart, int64_t yend,
                 std::function<void(int id, int64_t i, int64_t j)>&& task)
{
    parallel_for_chunked_2D (xstart, xend, 0, ystart, yend, 0,
            [&task](int id, int64_t xb, int64_t xe, int64_t yb, int64_t ye) {
        for (auto y = yb; y < ye; ++y)
            for (auto x = xb; x < xe; ++x)
                task (id, x, y);
    });
}



/// parallel_for, for a task that takes an int threadid and int64_t x & y
/// indices, running all of:
///    task (xstart, ystart);
///    ...
///    task (xend-1, ystart);
///    task (xstart, ystart+1);
///    task (xend-1, ystart+1);
///    ...
///    task (xend-1, yend-1);
inline void
parallel_for_2D (int64_t xstart, int64_t xend,
                 int64_t ystart, int64_t yend,
                 std::function<void(int64_t i, int64_t j)>&& task)
{
    parallel_for_chunked_2D (xstart, xend, 0, ystart, yend, 0,
            [&task](int id, int64_t xb, int64_t xe, int64_t yb, int64_t ye) {
        for (auto y = yb; y < ye; ++y)
            for (auto x = xb; x < xe; ++x)
                task (x, y);
    });
}



// DEPRECATED(1.8): This version accidentally accepted chunksizes that
// weren't used. Preserve for a version to not break 3rd party apps.
inline void
parallel_for_2D (int64_t xstart, int64_t xend, int64_t xchunksize,
                 int64_t ystart, int64_t yend, int64_t ychunksize,
                 std::function<void(int id, int64_t i, int64_t j)>&& task)
{
    parallel_for_2D (xstart, xend, ystart, yend,
                     std::forward<std::function<void(int,int64_t,int64_t)>>(task));
}

OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_PARALLEL_H
