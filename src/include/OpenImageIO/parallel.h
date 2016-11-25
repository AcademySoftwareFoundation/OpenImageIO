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

#include "thread.h"


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
    for (task_set<void> ts (pool); first != last; ++first)
        ts.push (pool->push ([&](int id){ f(*first); }));
    return std::move(f);
}


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_PARALLEL_H
