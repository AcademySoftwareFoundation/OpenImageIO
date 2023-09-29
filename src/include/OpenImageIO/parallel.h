// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <algorithm>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <OpenImageIO/function_view.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/thread.h>


OIIO_NAMESPACE_BEGIN

/// Split strategies
/// DEPRECATED(2.4)
enum SplitDir { Split_X, Split_Y, Split_Z, Split_Biggest, Split_Tile };


/// Encapsulation of options that control parallel_for() and
/// parallel_image().
/// DEPRECATED(2.4)
class parallel_options {
public:
    parallel_options(int maxthreads = 0, SplitDir splitdir = Split_Y,
                     size_t minitems = 16384)
        : maxthreads(maxthreads)
        , splitdir(splitdir)
        , minitems(minitems)
    {
    }
    parallel_options(string_view name, int maxthreads = 0,
                     SplitDir splitdir = Split_Y, size_t minitems = 16384)
        : maxthreads(maxthreads)
        , splitdir(splitdir)
        , minitems(minitems)
        , name(name)
    {
    }

    // Fix up all the TBD parameters:
    // * If no pool was specified, use the default pool.
    // * If no max thread count was specified, use the pool size.
    // * If the calling thread is itself in the pool and the recursive flag
    //   was not turned on, just use one thread.
    void resolve()
    {
        if (pool == nullptr)
            pool = default_thread_pool();
        if (maxthreads <= 0)
            maxthreads = pool->size() + 1;  // pool size + caller
        if (!recursive && pool->is_worker())
            maxthreads = 1;
    }

    bool singlethread() const { return maxthreads == 1; }

    int maxthreads    = 0;        // Max threads (0 = use all)
    SplitDir splitdir = Split_Y;  // Primary split direction
    bool recursive    = false;    // Allow thread pool recursion
    size_t minitems   = 16384;    // Min items per task
    thread_pool* pool = nullptr;  // If non-NULL, custom thread pool
    string_view name;             // For debugging
};



#define OIIO_PARALLEL_PAROPT

/// Encapsulation of options that control parallel_for() and
/// parallel_image().
class OIIO_UTIL_API paropt {
public:
    enum class ParStrategy : short { Default = 0, TryTBB, OIIOpool };
    enum class SplitDir : short { X, Y, Z, Biggest, Tile };

    constexpr paropt(int maxthreads = 0, SplitDir splitdir = SplitDir::Y,
                     size_t minitems = 1024) noexcept
        : m_maxthreads(maxthreads)
        , m_splitdir(splitdir)
        , m_minitems(minitems)
    {
    }
    paropt(string_view name, int maxthreads = 0,
           SplitDir splitdir = SplitDir::Y, size_t minitems = 1024) noexcept
        : paropt(maxthreads, splitdir, minitems)
    {
        // m_name = name;
    }

    constexpr paropt(ParStrategy strat) noexcept
        : m_strategy(strat)
    {
    }

    constexpr paropt(int maxthreads, ParStrategy strat) noexcept
        : m_maxthreads(maxthreads)
        , m_strategy(strat)
    {
    }

    // For back compatibility
    paropt(const parallel_options& po) noexcept
        : paropt(po.name, po.maxthreads, SplitDir(po.splitdir), po.minitems)
    {
        m_recursive = po.recursive;
        m_pool      = po.pool;
    }

    // Fix up all the TBD parameters:
    // * If no pool was specified, use the default pool.
    // * If no max thread count was specified, use the pool size.
    // * If the calling thread is itself in the pool and the recursive flag
    //   was not turned on, just use one thread.
    void resolve();

    constexpr bool singlethread() const noexcept { return m_maxthreads == 1; }

    constexpr int maxthreads() const noexcept { return m_maxthreads; }
    paropt& maxthreads(int m) noexcept
    {
        m_maxthreads = m;
        return *this;
    }

    constexpr SplitDir splitdir() const noexcept { return m_splitdir; }
    paropt& splitdir(SplitDir s) noexcept
    {
        m_splitdir = s;
        return *this;
    }

    constexpr bool recursive() const noexcept { return m_recursive; }
    paropt& recursive(bool r) noexcept
    {
        m_recursive = r;
        return *this;
    }

    constexpr int minitems() const noexcept { return m_minitems; }
    paropt& minitems(int m) noexcept
    {
        m_minitems = m;
        return *this;
    }

    thread_pool* pool() const noexcept { return m_pool; }
    paropt& pool(thread_pool* p) noexcept
    {
        m_pool = p;
        return *this;
    }

    constexpr ParStrategy strategy() const noexcept { return m_strategy; }
    paropt& strategy(ParStrategy s) noexcept
    {
        m_strategy = s;
        return *this;
    }

private:
    int m_maxthreads       = 0;  // Max threads (0 = use all)
    ParStrategy m_strategy = ParStrategy::Default;
    SplitDir m_splitdir    = SplitDir::Y;  // Primary split direction
    size_t m_minitems      = 16384;        // Min items per task
    thread_pool* m_pool    = nullptr;      // If non-NULL, custom thread pool
    bool m_recursive       = false;        // Allow thread pool recursion
};



/// Parallel "for" loop, chunked: for a task that takes an int64_t
/// [begin,end) range, break it into non-overlapping sections that run in
/// parallel:
///
///    task (begin, begin+chunksize);
///    task (begin+chunksize, begin+2*chunksize);
///    ...
///    task (begin+n*chunksize, end);
///
/// and wait for them all to complete.
///
/// If chunksize is 0, a chunksize will be chosen to divide the range into
/// a number of chunks equal to the twice number of threads in the queue.
/// (We do this to offer better load balancing than if we used exactly the
/// thread count.)
OIIO_UTIL_API void
parallel_for_chunked(int64_t begin, int64_t end, int64_t chunksize,
                     std::function<void(int64_t, int64_t)>&& task,
                     paropt opt = paropt(0, paropt::SplitDir::Y, 1));



/// Parallel "for" loop, for a task that takes a single integer index, run
/// it on all indices on the range [begin,end):
///
///    task (begin);
///    task (begin+1);
///    ...
///    task (end-1);
///
/// Conceptually, it behaves as if each index gets called separately, but
/// actually each thread will iterate over some chunk of adjacent indices
/// (to aid data coherence and minimize the amount of thread queue
/// diddling). The chunk size is chosen automatically.
OIIO_UTIL_API void
parallel_for(int32_t begin, int32_t end, function_view<void(int32_t)> task,
             paropt opt = 0);

OIIO_UTIL_API void
parallel_for(int64_t begin, int64_t end, function_view<void(int64_t)> task,
             paropt opt = 0);

OIIO_UTIL_API void
parallel_for(uint32_t begin, uint32_t end, function_view<void(uint32_t)> task,
             paropt opt = 0);

OIIO_UTIL_API void
parallel_for(uint64_t begin, uint64_t end, function_view<void(uint64_t)> task,
             paropt opt = 0);


/// Parallel "for" loop, for a task that takes an integer range, run it
/// on all indices on the range [begin,end):
///
///    task (begin, i1);
///    task (i1+1, i2);
///    ...
///    task (iN+1, end);
///
/// The chunk sizes will be chosen automatically, and are not guaranteed
/// to all be the same size.
OIIO_UTIL_API void
parallel_for_range(int32_t begin, int32_t end,
                   std::function<void(int32_t, int32_t)>&& task,
                   paropt opt = 0);

OIIO_UTIL_API void
parallel_for_range(int64_t begin, int64_t end,
                   std::function<void(int64_t, int64_t)>&& task,
                   paropt opt = 0);

OIIO_UTIL_API void
parallel_for_range(uint32_t begin, uint32_t end,
                   std::function<void(uint32_t, uint32_t)>&& task,
                   paropt opt = 0);

OIIO_UTIL_API void
parallel_for_range(uint64_t begin, uint64_t end,
                   std::function<void(uint64_t, uint64_t)>&& task,
                   paropt opt = 0);


/// Parallel "for" loop, chunked: for a task that takes a 2D [begin,end)
/// range and chunk sizes, subdivide that range and run in parallel:
///
///    task (begin, begin+chunksize);
///    task (begin+chunksize, begin+2*chunksize);
///    ...
///    task (begin+n*chunksize, end);
///
/// and wait for them all to complete.
///
/// If chunksize is 0, a chunksize will be chosen to divide the range into
/// a number of chunks equal to the twice number of threads in the queue.
/// (We do this to offer better load balancing than if we used exactly the
/// thread count.)
OIIO_UTIL_API void
parallel_for_chunked_2D(int64_t xbegin, int64_t xend, int64_t xchunksize,
                        int64_t ybegin, int64_t yend, int64_t ychunksize,
                        std::function<void(int64_t xbeg, int64_t xend,
                                           int64_t ybeg, int64_t yend)>&& task,
                        paropt opt = 0);



/// parallel_for, for a task that takes an int threadid and int64_t x & y
/// indices, running all of:
///    task (xbegin, ybegin);
///    ...
///    task (xend-1, ybegin);
///    task (xbegin, ybegin+1);
///    task (xend-1, ybegin+1);
///    ...
///    task (xend-1, yend-1);
OIIO_UTIL_API void
parallel_for_2D(int64_t xbegin, int64_t xend, int64_t ybegin, int64_t yend,
                std::function<void(int64_t x, int64_t y)>&& task,
                paropt opt = 0);



#if OIIO_VERSION < OIIO_MAKE_VERSION(3, 0, 0)

// Deprecated versions of parallel loops where the task functions take a
// thread ID in addition to the range. These are deprecated as of OIIO 2.3,
// will warn about deprecation starting in OIIO 2.4, and will be removed
// starting with OIIO 3.0.

// OIIO_DEPRECATED("Use tasks that don't take a thread ID (2.3)")
OIIO_UTIL_API void
parallel_for_chunked(int64_t begin, int64_t end, int64_t chunksize,
                     std::function<void(int id, int64_t b, int64_t e)>&& task,
                     paropt opt = paropt(0, paropt::SplitDir::Y, 1));

// OIIO_DEPRECATED("Use tasks that don't take a thread ID (2.3)")
OIIO_UTIL_API void
parallel_for(int64_t begin, int64_t end,
             std::function<void(int id, int64_t index)>&& task,
             paropt opt = paropt(0, paropt::SplitDir::Y, 1));

// OIIO_DEPRECATED("Use tasks that don't take a thread ID (2.3)")
OIIO_UTIL_API void
parallel_for_chunked_2D(
    int64_t xbegin, int64_t xend, int64_t xchunksize, int64_t ybegin,
    int64_t yend, int64_t ychunksize,
    std::function<void(int id, int64_t, int64_t, int64_t, int64_t)>&& task,
    paropt opt = 0);

// OIIO_DEPRECATED("Use tasks that don't take a thread ID (2.3)")
inline void
parallel_for_2D(int64_t xbegin, int64_t xend, int64_t ybegin, int64_t yend,
                std::function<void(int id, int64_t i, int64_t j)>&& task,
                paropt opt = 0)
{
    parallel_for_chunked_2D(
        xbegin, xend, 0, ybegin, yend, 0,
        [&task](int id, int64_t xb, int64_t xe, int64_t yb, int64_t ye) {
            for (auto y = yb; y < ye; ++y)
                for (auto x = xb; x < xe; ++x)
                    task(id, x, y);
        },
        opt);
}

// Deprecated parallel_for_each. We never used it and I decided I didn't
// like the implementation and didn't want its guts exposed any more. For
// compatibility (just in case somebody has used it), implement it serially
// so that it's correct, even if it's not fast. It will eventually be
// removed.
template<class InputIt, class UnaryFunction>
// OIIO_DEPRECATED("Don't use this (2.3)")
UnaryFunction
parallel_for_each(InputIt begin, InputIt end, UnaryFunction f,
                  paropt opt = paropt(0, paropt::SplitDir::Y, 1))
{
    return std::for_each(begin, end, f);
}

// DEPRECATED(1.8): This version accidentally accepted chunksizes that
// weren't used. Preserve for a version to not break 3rd party apps.
OIIO_DEPRECATED("Use the version without chunk sizes (1.8)")
inline void
parallel_for_2D(int64_t xbegin, int64_t xend, int64_t /*xchunksize*/,
                int64_t ybegin, int64_t yend, int64_t /*ychunksize*/,
                std::function<void(int id, int64_t i, int64_t j)>&& task)
{
    parallel_for_2D(xbegin, xend, ybegin, yend, std::move(task));
}

#endif /* Deprecated functions */


OIIO_NAMESPACE_END
