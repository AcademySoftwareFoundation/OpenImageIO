// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


// This implementation of thread_pool is based on CTPL.
// We've made a variety of changes (we hope improvements) ourselves to cater
// it to our needs.
//
// The original CTPL is:
// https://github.com/vit-vit/CTPL
// Copyright (C) 2014 by Vitaliy Vitsentiy
// Licensed with Apache 2.0
// (see https://github.com/vit-vit/CTPL/blob/master/LICENSE)


#if defined(_MSC_VER)
#    define _ENABLE_ATOMIC_ALIGNMENT_FIX /* Avoid MSVS error, ugh */
#endif

#include <exception>
#include <functional>
#include <future>
#include <memory>

#include <OpenImageIO/parallel.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>

#include <boost/container/flat_map.hpp>

#if 0

// Use boost::lockfree::queue for the task queue
#    include <boost/lockfree/queue.hpp>
template<typename T> using Queue = boost::lockfree::queue<T>;

#else

#    include <queue>

namespace {

template<typename T> class Queue {
public:
    Queue(int /*size*/) {}
    bool push(T const& value)
    {
        std::unique_lock<Mutex> lock(this->mutex);
        this->q.push(value);
        return true;
    }
    // deletes the retrieved element, do not use for non integral types
    bool pop(T& v)
    {
        std::unique_lock<Mutex> lock(this->mutex);
        if (this->q.empty())
            return false;
        v = this->q.front();
        this->q.pop();
        return true;
    }
    bool empty()
    {
        std::unique_lock<Mutex> lock(this->mutex);
        return this->q.empty();
    }
    size_t size()
    {
        std::unique_lock<Mutex> lock(this->mutex);
        return q.size();
    }

private:
    typedef OIIO::spin_mutex Mutex;
    std::queue<T> q;
    Mutex mutex;
};

}  // namespace

#endif


OIIO_NAMESPACE_BEGIN

static int
threads_default()
{
    int n = Strutil::from_string<int>(Sysutil::getenv("OPENIMAGEIO_THREADS"));
    if (n < 1)
        n = Sysutil::hardware_concurrency();
    return n;
}



class thread_pool::Impl {
public:
    Impl(int nThreads = 0, int queueSize = 1024)
        : q(queueSize)
    {
        this->init();
        this->resize(nThreads);
    }

    // the destructor waits for all the functions in the queue to be finished
    ~Impl() { this->stop(true); }

    // get the number of running threads in the pool
    int size() const
    {
        OIIO_DASSERT(m_size == static_cast<int>(this->threads.size()));
        return m_size;
    }

    // number of idle threads
    int n_idle() const { return this->nWaiting; }

    std::thread& get_thread(int i) { return *this->threads[i]; }

    // change the number of threads in the pool
    // should be called from one thread, otherwise be careful to not interleave, also with this->stop()
    // nThreads must be >= 0
    void resize(int nThreads)
    {
        if (nThreads < 0)
            nThreads = std::max(1, int(threads_default()) - 1);
        if (!this->isStop && !this->isDone) {
            int oldNThreads = size();
            if (oldNThreads
                <= nThreads) {  // if the number of threads is increased
                this->threads.resize(nThreads);
                this->flags.resize(nThreads);
                for (int i = oldNThreads; i < nThreads; ++i) {
                    this->flags[i] = std::make_shared<std::atomic<bool>>(false);
                    this->set_thread(i);
                }
            } else {  // the number of threads is decreased
                for (int i = oldNThreads - 1; i >= nThreads; --i) {
                    *this->flags[i] = true;  // this thread will finish
                    this->terminating_threads.push_back(
                        std::move(this->threads[i]));
                    this->threads.erase(this->threads.begin() + i);
                }
                {
                    // stop the detached threads that were waiting
                    std::unique_lock<std::mutex> lock(this->mutex);
                    this->cv.notify_all();
                }
                this->threads.resize(
                    nThreads);  // safe to delete because the threads are detached
                this->flags.resize(
                    nThreads);  // safe to delete because the threads have copies of shared_ptr of the flags, not originals
            }
        }
        m_size = nThreads;
    }

    // empty the queue
    void clear_queue()
    {
        std::function<void(int id)>* _f;
        while (this->q.pop(_f))
            delete _f;  // empty the queue
    }

    // pops a functional wraper to the original function
    std::function<void(int)> pop()
    {
        std::function<void(int id)>* _f = nullptr;
        this->q.pop(_f);
        std::unique_ptr<std::function<void(int id)>> func(
            _f);  // at return, delete the function even if an exception occurred
        std::function<void(int)> f;
        if (_f)
            f = *_f;
        return f;
    }


    // wait for all computing threads to finish and stop all threads
    // may be called asyncronously to not pause the calling thread while waiting
    // if isWait == true, all the functions in the queue are run, otherwise the queue is cleared without running the functions
    void stop(bool isWait = false)
    {
        if (!isWait) {
            if (this->isStop)
                return;
            this->isStop = true;
            for (int i = 0, n = this->size(); i < n; ++i) {
                *this->flags[i] = true;  // command the threads to stop
            }
            this->clear_queue();  // empty the queue
        } else {
            if (this->isDone || this->isStop)
                return;
            this->isDone = true;  // give the waiting threads a command to finish
        }

#if defined(_WIN32)
        // When the static variable in default_thread_pool() is destroyed during DLL unloading,
        // the thread_pool destructor is called but the threads are already terminated.
        // So it is illegal to communicate with those other threads at this point.
        // Checking Windows native thread status allows to detect this specific scenario and avoid an unnecessary call
        // to this->cv.notify_all() which creates a deadlock (noticed only on Windows 7 but still unsafe in other versions).
        bool hasTerminatedThread
            = std::any_of(this->threads.begin(), this->threads.end(),
                          [](std::unique_ptr<std::thread>& t) {
                              DWORD rcode;
                              GetExitCodeThread((HANDLE)t->native_handle(),
                                                &rcode);
                              return rcode != STILL_ACTIVE;
                          });

        if (!hasTerminatedThread)
#endif
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_all();  // stop all waiting threads
        }
        // wait for the computing threads to finish
        for (auto& thread : this->threads) {
            if (thread->joinable())
                thread->join();
        }
        // wait for the terminated threads to finish
        for (auto& thread : this->terminating_threads) {
            if (thread->joinable())
                thread->join();
        }
        // if there were no threads in the pool but some functors in the queue, the functors are not deleted by the threads
        // therefore delete them here
        this->clear_queue();
        this->threads.clear();
        this->terminating_threads.clear();
        this->flags.clear();
    }

    void push_queue_and_notify(std::function<void(int id)>* f)
    {
        this->q.push(f);
        std::unique_lock<std::mutex> lock(this->mutex);
        this->cv.notify_one();
    }

    // If any tasks are on the queue, pop and run one with the calling
    // thread.
    bool run_one_task(std::thread::id id)
    {
        std::function<void(int)>* f = nullptr;
        bool isPop                  = this->q.pop(f);
        if (isPop) {
            OIIO_DASSERT(f);
            std::unique_ptr<std::function<void(int id)>> func(
                f);  // at return, delete the function even if an exception occurred
            register_worker(id);
            (*f)(-1);
            deregister_worker(id);
        } else {
            OIIO_DASSERT(f == nullptr);
        }
        return isPop;
    }

    void register_worker(std::thread::id id)
    {
        spin_lock lock(m_worker_threadids_mutex);
        m_worker_threadids[id] += 1;
    }
    void deregister_worker(std::thread::id id)
    {
        spin_lock lock(m_worker_threadids_mutex);
        m_worker_threadids[id] -= 1;
    }
    bool is_worker(std::thread::id id) const
    {
        spin_lock lock(m_worker_threadids_mutex);
        return m_worker_threadids[id] != 0;
    }

    size_t jobs_in_queue() const { return q.size(); }

    bool very_busy() const { return jobs_in_queue() > size_t(4 * m_size); }

private:
    Impl(const Impl&) = delete;
    Impl(Impl&&)      = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    void set_thread(int i)
    {
        std::shared_ptr<std::atomic<bool>> flag(
            this->flags[i]);  // a copy of the shared ptr to the flag
        auto f = [this, i, flag /* a copy of the shared ptr to the flag */]() {
            register_worker(std::this_thread::get_id());
            std::atomic<bool>& _flag = *flag;
            std::function<void(int id)>* _f;
            bool isPop = this->q.pop(_f);
            while (true) {
                while (isPop) {  // if there is anything in the queue
                    std::unique_ptr<std::function<void(int id)>> func(
                        _f);  // at return, delete the function even if an exception occurred
                    (*_f)(i);
                    if (_flag) {
                        // the thread is wanted to stop, return even if the queue is not empty yet
                        return;
                    } else {
                        isPop = this->q.pop(_f);
                    }
                }
                // the queue is empty here, wait for the next command
                std::unique_lock<std::mutex> lock(this->mutex);
                ++this->nWaiting;
                this->cv.wait(lock, [this, &_f, &isPop, &_flag]() {
                    isPop = this->q.pop(_f);
                    return isPop || this->isDone || _flag;
                });
                --this->nWaiting;
                if (!isPop)
                    break;  // if the queue is empty and this->isDone == true or *flag then return
            }
            deregister_worker(std::this_thread::get_id());
        };
        this->threads[i].reset(
            new std::thread(f));  // compiler may not support std::make_unique()
    }

    void init()
    {
        this->nWaiting = 0;
        this->isStop   = false;
        this->isDone   = false;
    }

    std::vector<std::unique_ptr<std::thread>> threads;
    std::vector<std::unique_ptr<std::thread>> terminating_threads;
    std::vector<std::shared_ptr<std::atomic<bool>>> flags;
    mutable Queue<std::function<void(int id)>*> q;
    std::atomic<bool> isDone;
    std::atomic<bool> isStop;
    std::atomic<int> nWaiting;  // how many threads are waiting
    int m_size { 0 };           // Number of threads in the queue
    std::mutex mutex;
    std::condition_variable cv;
    mutable boost::container::flat_map<std::thread::id, int> m_worker_threadids;
    mutable spin_mutex m_worker_threadids_mutex;
};



thread_pool::thread_pool(int nthreads)
    : m_impl(new Impl(nthreads))
{
    resize(nthreads);
}



thread_pool::~thread_pool()
{
    // Will implicitly delete the impl
}



int
thread_pool::size() const
{
    return m_impl->size();
}



void
thread_pool::resize(int nthreads)
{
    m_impl->resize(nthreads);
}



int
thread_pool::idle() const
{
    return m_impl->n_idle();
}



size_t
thread_pool::jobs_in_queue() const
{
    return m_impl->jobs_in_queue();
}



bool
thread_pool::run_one_task(std::thread::id id)
{
    return m_impl->run_one_task(id);
}



void
thread_pool::push_queue_and_notify(std::function<void(int id)>* f)
{
    m_impl->push_queue_and_notify(f);
}



/// DEPRECATED(2.1) -- use is_worker() instead.
bool
thread_pool::this_thread_is_in_pool() const
{
    return is_worker();
}



void
thread_pool::register_worker(std::thread::id id)
{
    m_impl->register_worker(id);
}

void
thread_pool::deregister_worker(std::thread::id id)
{
    m_impl->deregister_worker(id);
}

bool
thread_pool::is_worker(std::thread::id id) const
{
    return m_impl->is_worker(id);
}


// DEPRECATED(2.1)
bool
thread_pool::is_worker(std::thread::id id)
{
    return m_impl->is_worker(id);
}


bool
thread_pool::very_busy() const
{
    return m_impl->very_busy();
}



thread_pool*
default_thread_pool()
{
    static std::unique_ptr<thread_pool> shared_pool(new thread_pool);
    return shared_pool.get();
}



void
task_set::wait_for_task(size_t taskindex, bool block)
{
    OIIO_DASSERT(submitter() == std::this_thread::get_id());
    if (taskindex >= m_futures.size())
        return;  // nothing to wait for
    auto& f(m_futures[taskindex]);
    if (block || m_pool->is_worker(m_submitter_thread)) {
        // Block on completion of all the task and don't try to do any
        // of the work with the calling thread.
        f.wait();
        return;
    }
    // If we made it here, we want to allow the calling thread to help
    // do pool work if it's waiting around for a while.
    const std::chrono::milliseconds wait_time(0);
    int tries = 0;
    while (1) {
        // Asking future.wait_for for 0 time just checks the status.
        if (f.wait_for(wait_time) == std::future_status::ready)
            return;  // task has completed
        // We're still waiting for the task to complete. What next?
        if (++tries < 4) {  // First few times,
            pause(4);       //   just busy-wait, check status again
            continue;
        }
        // Since we're waiting, try to run a task ourselves to help
        // with the load. If none is available, just yield schedule.
        if (!m_pool->run_one_task(m_submitter_thread)) {
            // We tried to do a task ourselves, but there weren't any
            // left, so just wait for the rest to finish.
            yield();
        }
    }
}



void
task_set::wait(bool block)
{
    OIIO_DASSERT(submitter() == std::this_thread::get_id());
    const std::chrono::milliseconds wait_time(0);
    if (m_pool->is_worker(m_submitter_thread))
        block = true;  // don't get into recursive work stealing
    if (block == false) {
        int tries = 0;
        while (1) {
            bool all_finished = true;
            int nfutures = 0, finished = 0;
            for (auto&& f : m_futures) {
                // Asking future.wait_for for 0 time just checks the status.
                ++nfutures;
                auto status = f.wait_for(wait_time);
                if (status != std::future_status::ready)
                    all_finished = false;
                else
                    ++finished;
            }
            if (all_finished)  // All futures are ready? We're done.
                break;
            // We're still waiting on some tasks to complete. What next?
            if (++tries < 4) {  // First few times,
                pause(4);       //   just busy-wait, check status again
                continue;
            }
            // Since we're waiting, try to run a task ourselves to help
            // with the load. If none is available, just yield schedule.
            if (!m_pool->run_one_task(m_submitter_thread)) {
                // We tried to do a task ourselves, but there weren't any
                // left, so just wait for the rest to finish.
#if 1
                yield();
#else
                // FIXME -- as currently written, if we see an empty queue
                // but we're still waiting for the tasks in our set to end,
                // we will keep looping and potentially ourselves do work
                // that was part of another task set. If there a benefit to,
                // once we see an empty queue, only waiting for the existing
                // tasks to finish and not altruistically executing any more
                // tasks?  This is how we would take the exit now:
                for (auto&& f : m_futures)
                    f.wait();
                break;
#endif
            }
        }
    } else {
        // If block is true, just block on completion of all the tasks
        // and don't try to do any of the work with the calling thread.
        for (auto&& f : m_futures)
            f.wait();
    }
#ifndef NDEBUG
    check_done();
#endif
}



// Helper function to keep track of the recursve depth of our use of the
// thread pool. Call with the adjustment (i.e., parallel_recursive_depth(1)
// to enter, parallel_recursive_depth(-1) to exit), and it will return the
// new value. Call with default args (0) to just return the current depth.
static int
parallel_recursive_depth(int change = 0)
{
    thread_local int depth = 0;  // let's only allow one level of parallel work
    depth += change;
    return depth;
}



void
parallel_for_chunked(int64_t start, int64_t end, int64_t chunksize,
                     std::function<void(int id, int64_t b, int64_t e)>&& task,
                     parallel_options opt)
{
    if (parallel_recursive_depth(1) > 1)
        opt.maxthreads = 1;
    opt.resolve();
    chunksize = std::min(chunksize, end - start);
    if (chunksize < 1) {           // If caller left chunk size to us...
        if (opt.singlethread()) {  // Single thread: do it all in one shot
            chunksize = end - start;
        } else {  // Multithread: choose a good chunk size
            int p     = std::max(1, 2 * opt.maxthreads);
            chunksize = std::max(int64_t(opt.minitems), (end - start) / p);
        }
    }
    // N.B. If chunksize was specified, honor it, even for the single
    // threaded case.
    for (task_set ts(opt.pool); start < end; start += chunksize) {
        int64_t e = std::min(end, start + chunksize);
        if (e == end || opt.singlethread() || opt.pool->very_busy()) {
            // For the last (or only) subtask, or if we are using just one
            // thread, or if the pool is already oversubscribed, do it
            // ourselves and avoid messing with the queue or handing off
            // between threads.
            task(-1, start, e);
        } else {
            ts.push(opt.pool->push(task, start, e));
        }
    }
    parallel_recursive_depth(-1);
}



void
parallel_for_chunked_2D(
    int64_t xstart, int64_t xend, int64_t xchunksize, int64_t ystart,
    int64_t yend, int64_t ychunksize,
    std::function<void(int id, int64_t, int64_t, int64_t, int64_t)>&& task,
    parallel_options opt)
{
    if (parallel_recursive_depth(1) > 1)
        opt.maxthreads = 1;
    opt.resolve();
    if (opt.singlethread()
        || (xchunksize >= (xend - xstart) && ychunksize >= (yend - ystart))
        || opt.pool->very_busy()) {
        task(-1, xstart, xend, ystart, yend);
        parallel_recursive_depth(-1);
        return;
    }
    if (ychunksize < 1)
        ychunksize = std::max(int64_t(1),
                              (yend - ystart) / (2 * opt.maxthreads));
    if (xchunksize < 1) {
        int64_t ny = std::max(int64_t(1), (yend - ystart) / ychunksize);
        int64_t nx = std::max(int64_t(1), opt.maxthreads / ny);
        xchunksize = std::max(int64_t(1), (xend - xstart) / nx);
    }
    task_set ts(opt.pool);
    for (auto y = ystart; y < yend; y += ychunksize) {
        int64_t ychunkend = std::min(yend, y + ychunksize);
        for (auto x = xstart; x < xend; x += xchunksize) {
            int64_t xchunkend = std::min(xend, x + xchunksize);
            ts.push(opt.pool->push(task, x, xchunkend, y, ychunkend));
        }
    }
    parallel_recursive_depth(-1);
}


OIIO_NAMESPACE_END
