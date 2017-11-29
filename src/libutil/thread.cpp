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
#define _ENABLE_ATOMIC_ALIGNMENT_FIX /* Avoid MSVS error, ugh */
#endif

#include <exception>
#include <functional>
#include <future>
#include <memory>

#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/strutil.h>

#include <boost/thread/tss.hpp>
#include <boost/container/flat_map.hpp>

#if 0

// Use boost::lockfree::queue for the task queue
#include <boost/lockfree/queue.hpp>
template<typename T> using Queue = boost::lockfree::queue<T>;

#else

#include <queue>

namespace {

template <typename T>
class Queue {
public:
    Queue (int size) { }
    bool push(T const & value) {
        std::unique_lock<Mutex> lock(this->mutex);
        this->q.push(value);
        return true;
    }
    // deletes the retrieved element, do not use for non integral types
    bool pop(T & v) {
        std::unique_lock<Mutex> lock(this->mutex);
        if (this->q.empty())
            return false;
        v = this->q.front();
        this->q.pop();
        return true;
    }
    bool empty() {
        std::unique_lock<Mutex> lock(this->mutex);
        return this->q.empty();
    }
private:
    typedef OIIO::spin_mutex Mutex;
    std::queue<T> q;
    Mutex mutex;
};

}

#endif


OIIO_NAMESPACE_BEGIN

static int
threads_default ()
{
    int n = Strutil::from_string<int>(Sysutil::getenv("OPENIMAGEIO_THREADS"));
    if (n < 1)
        n = Sysutil::hardware_concurrency();
    return n;
}



class thread_pool::Impl {
public:
    Impl (int nThreads = 0, int queueSize = 1024) : q(queueSize) { this->init(); this->resize(nThreads); }

    // the destructor waits for all the functions in the queue to be finished
    ~Impl () {
        this->stop(true);
    }

    // get the number of running threads in the pool
    int size() const { return static_cast<int>(this->threads.size()); }

    // number of idle threads
    int n_idle() const { return this->nWaiting; }

    std::thread & get_thread(int i) { return *this->threads[i]; }

    // change the number of threads in the pool
    // should be called from one thread, otherwise be careful to not interleave, also with this->stop()
    // nThreads must be >= 0
    void resize(int nThreads) {
        if (nThreads < 0)
            nThreads = std::max (1, int(threads_default()) - 1);
        if (!this->isStop && !this->isDone) {
            int oldNThreads = static_cast<int>(this->threads.size());
            if (oldNThreads <= nThreads) {  // if the number of threads is increased
                this->threads.resize(nThreads);
                this->flags.resize(nThreads);
                for (int i = oldNThreads; i < nThreads; ++i) {
                    this->flags[i] = std::make_shared<std::atomic<bool>>(false);
                    this->set_thread(i);
                }
            }
            else {  // the number of threads is decreased
                for (int i = oldNThreads - 1; i >= nThreads; --i) {
                    *this->flags[i] = true;  // this thread will finish
                    this->threads[i]->detach();
                }
                {
                    // stop the detached threads that were waiting
                    std::unique_lock<std::mutex> lock(this->mutex);
                    this->cv.notify_all();
                }
                this->threads.resize(nThreads);  // safe to delete because the threads are detached
                this->flags.resize(nThreads);  // safe to delete because the threads have copies of shared_ptr of the flags, not originals
            }
        }
    }

    // empty the queue
    void clear_queue() {
        std::function<void(int id)> * _f;
        while (this->q.pop(_f))
            delete _f;  // empty the queue
    }

    // pops a functional wraper to the original function
    std::function<void(int)> pop() {
        std::function<void(int id)> * _f = nullptr;
        this->q.pop(_f);
        std::unique_ptr<std::function<void(int id)>> func(_f);  // at return, delete the function even if an exception occurred
        std::function<void(int)> f;
        if (_f)
            f = *_f;
        return f;
    }


    // wait for all computing threads to finish and stop all threads
    // may be called asyncronously to not pause the calling thread while waiting
    // if isWait == true, all the functions in the queue are run, otherwise the queue is cleared without running the functions
    void stop(bool isWait = false) {
        if (!isWait) {
            if (this->isStop)
                return;
            this->isStop = true;
            for (int i = 0, n = this->size(); i < n; ++i) {
                *this->flags[i] = true;  // command the threads to stop
            }
            this->clear_queue();  // empty the queue
        }
        else {
            if (this->isDone || this->isStop)
                return;
            this->isDone = true;  // give the waiting threads a command to finish
        }
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_all();  // stop all waiting threads
        }
        for (auto& thread : this->threads) {  // wait for the computing threads to finish
            if (thread->joinable())
                thread->join();
        }
        // if there were no threads in the pool but some functors in the queue, the functors are not deleted by the threads
        // therefore delete them here
        this->clear_queue();
        this->threads.clear();
        this->flags.clear();
    }

    template<typename F, typename... Rest>
    auto push(F && f, Rest&&... rest) ->std::future<decltype(f(0, rest...))> {
        auto pck = std::make_shared<std::packaged_task<decltype(f(0, rest...))(int)>>(
            std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Rest>(rest)...)
        );
        if (size() < 1 || is_worker(std::this_thread::get_id())) {
            (*pck)(-1); // No worker threads, run it with the calling thread
        } else {
            auto _f = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
            });
            this->q.push(_f);
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_one();
        }
        return pck->get_future();
    }

    // run the user's function that excepts argument int - id of the running thread. returned value is templatized
    // operator returns std::future, where the user can get the result and rethrow the catched exceptins
    template<typename F>
    auto push(F && f) ->std::future<decltype(f(0))> {
        auto pck = std::make_shared<std::packaged_task<decltype(f(0))(int)>>(std::forward<F>(f));
        if (size() < 1 || is_worker(std::this_thread::get_id())) {
            (*pck)(-1); // No worker threads, run it with the calling thread
        } else {
            auto _f = new std::function<void(int id)>([pck](int id) {
                (*pck)(id);
            });
            this->q.push(_f);
            std::unique_lock<std::mutex> lock(this->mutex);
            this->cv.notify_one();
        }
        return pck->get_future();
    }

    void push_queue_and_notify (std::function<void(int id)> *f) {
        this->q.push(f);
        std::unique_lock<std::mutex> lock(this->mutex);
        this->cv.notify_one();
    }

    // If any tasks are on the queue, pop and run one with the calling
    // thread.
    bool run_one_task (std::thread::id id) {
        std::function<void(int)> * f = nullptr;
        bool isPop = this->q.pop(f);
        if (isPop) {
            DASSERT (f);
            std::unique_ptr<std::function<void(int id)>> func(f);  // at return, delete the function even if an exception occurred
            register_worker (id);
            (*f)(-1);
            deregister_worker (id);
        } else {
            DASSERT (f == nullptr);
        }
        return isPop;
    }

    bool this_thread_is_in_pool () const {
        int *p = m_pool_members.get();
        return p && (*p);
    }

    void register_worker (std::thread::id id) {
        spin_lock lock (m_worker_threadids_mutex);
        m_worker_threadids[id] += 1;
    }
    void deregister_worker (std::thread::id id) {
        spin_lock lock (m_worker_threadids_mutex);
        m_worker_threadids[id] -= 1;
    }
    bool is_worker (std::thread::id id) {
        spin_lock lock (m_worker_threadids_mutex);
        return m_worker_threadids[id] != 0;
    }

private:
    Impl (const Impl  &) = delete;
    Impl (Impl  &&) = delete;
    Impl  & operator=(const Impl  &) = delete;
    Impl  & operator=(Impl  &&) = delete;

    void set_thread(int i) {
        std::shared_ptr<std::atomic<bool>> flag(this->flags[i]);  // a copy of the shared ptr to the flag
        auto f = [this, i, flag/* a copy of the shared ptr to the flag */]() {
            this->m_pool_members.reset (new int (1)); // I'm in the pool
            register_worker (std::this_thread::get_id());
            std::atomic<bool> & _flag = *flag;
            std::function<void(int id)> * _f;
            bool isPop = this->q.pop(_f);
            while (true) {
                while (isPop) {  // if there is anything in the queue
                    std::unique_ptr<std::function<void(int id)>> func(_f);  // at return, delete the function even if an exception occurred
                    (*_f)(i);
                    if (_flag) {
                        // the thread is wanted to stop, return even if the queue is not empty yet
                        this->m_pool_members.reset (); // I'm no longer in the pool
                        return;
                    }
                    else
                        isPop = this->q.pop(_f);
                }
                // the queue is empty here, wait for the next command
                std::unique_lock<std::mutex> lock(this->mutex);
                ++this->nWaiting;
                this->cv.wait(lock, [this, &_f, &isPop, &_flag](){ isPop = this->q.pop(_f); return isPop || this->isDone || _flag; });
                --this->nWaiting;
                if (!isPop)
                    break;  // if the queue is empty and this->isDone == true or *flag then return
            }
            this->m_pool_members.reset (); // I'm no longer in the pool
            deregister_worker (std::this_thread::get_id());
        };
        this->threads[i].reset(new std::thread(f));  // compiler may not support std::make_unique()
    }

    void init() { this->nWaiting = 0; this->isStop = false; this->isDone = false; }

    std::vector<std::unique_ptr<std::thread>> threads;
    std::vector<std::shared_ptr<std::atomic<bool>>> flags;
    mutable Queue<std::function<void(int id)> *> q;
    std::atomic<bool> isDone;
    std::atomic<bool> isStop;
    std::atomic<int> nWaiting;  // how many threads are waiting
    std::mutex mutex;
    std::condition_variable cv;
    boost::thread_specific_ptr<int> m_pool_members; // Who's in the pool
    boost::container::flat_map<std::thread::id,int> m_worker_threadids;
    spin_mutex m_worker_threadids_mutex;
};





thread_pool::thread_pool (int nthreads)
    : m_impl (new Impl (nthreads))
{
    resize (nthreads);
}



thread_pool::~thread_pool ()
{
    // Will implicitly delete the impl
}



int
thread_pool::size () const
{
    return m_impl->size();
}



void
thread_pool::resize (int nthreads)
{
    m_impl->resize (nthreads);
}



int
thread_pool::idle () const
{
    return m_impl->n_idle();
}



bool
thread_pool::run_one_task (std::thread::id id)
{
    return m_impl->run_one_task (id);
}



void
thread_pool::push_queue_and_notify (std::function<void(int id)> *f)
{
    m_impl->push_queue_and_notify (f);
}


bool
thread_pool::this_thread_is_in_pool () const
{
    return m_impl->this_thread_is_in_pool ();
}



void
thread_pool::register_worker (std::thread::id id)
{
    m_impl->register_worker (id);
}

void
thread_pool::deregister_worker (std::thread::id id)
{
    m_impl->deregister_worker (id);
}

bool
thread_pool::is_worker (std::thread::id id)
{
    return m_impl->is_worker (id);
}



thread_pool *
default_thread_pool ()
{
    static std::unique_ptr<thread_pool> shared_pool (new thread_pool);
    return shared_pool.get();
}


OIIO_NAMESPACE_END

