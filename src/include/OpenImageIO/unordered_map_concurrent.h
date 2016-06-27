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


#pragma once

#ifndef OPENIMAGEIO_UNORDERED_MAP_CONCURRENT_H
#define OPENIMAGEIO_UNORDERED_MAP_CONCURRENT_H

#include <OpenImageIO/thread.h>
#include <OpenImageIO/hash.h>
#include <OpenImageIO/dassert.h>

OIIO_NAMESPACE_BEGIN


/// unordered_map_concurrent provides an unordered_map replacement that
/// is optimized for concurrent access.  Its principle of operation is
/// similar to Java's ConcurrentHashMap.
///
/// With naive use of an unordered_map, multiple threads would have to
/// lock a mutex of some kind to control access to the map, either with
/// a unique full lock, or with a reader/writer lock.  But as the number
/// of threads contending for this shared resource rises, they end up
/// locking each other out and the map becomes a thread bottleneck.
///
/// unordered_map_concurrent solves this problem by internally splitting
/// the hash map into several disjoint bins, each of which is a standard
/// unordered_map.  For any given map item, the hash of its key
/// determines both the bin as well as its hashing within the bin (i.e.,
/// we break a big hash map into lots of little hash maps,
/// deterministically determined by the key).  Thus, we should expect
/// map entries to be spread more or less evenly among the bins.  There
/// is no mutex that locks the map as a whole; instead, each bin is
/// locked individually.  If the number of bins is larger than the
/// typical number of threads, most of the time two (or more) threads
/// accessing the map simultaneously will not be accessing the same bin,
/// and therefore will not be contending for the same lock.
///
/// unordered_map_concurrent provides an iterator which points to an
/// entry in the map and also knows which bin it is in and implicitly
/// holds a lock on the bin.  When the iterator is destroyed, the lock
/// on that bin is released.  When the iterator is incremented in such a
/// way that it transitions from the last entry of its current bin to
/// the first entry of the next bin, it will also release its current
/// lock and obtain a lock on the next bin.
///

template<class KEY, class VALUE, class HASH=boost::hash<KEY>,
         class PRED=std::equal_to<KEY>, size_t BINS=16,
         class BINMAP=unordered_map<KEY,VALUE,HASH,PRED> >
class unordered_map_concurrent {
public:
    typedef BINMAP BinMap_t;
    typedef typename BINMAP::iterator BinMap_iterator_t;

public:
    unordered_map_concurrent () { m_size = 0; }

    ~unordered_map_concurrent () {
//        for (size_t i = 0;  i < BINS;  ++i)
//            std::cout << "Bin " << i << ": " << m_bins[i].map.size() << "\n";
    }

    /// An unordered_map_concurrent::iterator points to a specific entry
    /// in the umc, and holds a lock to the bin the entry is in.
    class iterator {
    public:
        friend class unordered_map_concurrent<KEY,VALUE,HASH,PRED,BINS,BINMAP>;
    public:
        /// Construct an unordered_map_concurrent iterator that points
        /// to nothing.
        iterator (unordered_map_concurrent *umc = NULL)
            : m_umc(umc), m_bin(-1), m_locked(false) { }

        /// Copy constructor of an unordered_map_concurrent iterator
        /// transfers the lock (if held) to this.  Caveat: the copied
        /// iterator no longer holds the lock!
        iterator (const iterator &src) {
            m_umc = src.m_umc;
            m_bin = src.m_bin;
            m_biniterator = src.m_biniterator;
            m_locked = src.m_locked;
            // assignment transfers lock ownership
            *(const_cast<bool *>(&src.m_locked)) = false;
        }

        /// Destroying an unordered_map_concurrent iterator releases any
        /// bin locks it held.
        ~iterator () { clear(); }

        /// Totally invalidate this iterator -- point it to nothing
        /// (releasing any locks it may have had).
        void clear () {
            if (m_umc) {
                unbin ();
                m_umc = NULL;
            }
        }

        // Dereferencing returns a reference to the hash table entry the
        // iterator refers to.
        typename BinMap_t::value_type & operator* () {
            return *m_biniterator;
        }

        /// Dereferencing returns a reference to the hash table entry the
        /// iterator refers to.
        typename BinMap_t::value_type * operator-> () {
            return &(*m_biniterator);
        }

        /// Treating an iterator as a bool yields true if it points to a
        /// valid element of one of the bins of the map, false if it's
        /// equivalent to the end() iterator.
        operator bool() {
            return m_umc && m_bin >= 0 &&
                m_biniterator != m_umc->m_bins[m_bin].map.end();
        }

        /// Iterator assignment transfers ownership of any bin locks
        /// held by the operand.
        iterator& operator= (const iterator &src) {
            unbin();
            m_umc = src.m_umc;
            m_bin = src.m_bin;
            m_biniterator = src.m_biniterator;
            m_locked = src.m_locked;
            // assignment transfers lock ownership
            *(const_cast<bool *>(&src.m_locked)) = false;
            return *this;
        }

        bool operator== (const iterator &other) const {
            if (m_umc != other.m_umc)
                return false;
            if (m_bin == -1 && other.m_bin == -1)
                return true;
            return m_bin == other.m_bin &&
                m_biniterator == other.m_biniterator;
        }
        bool operator!= (const iterator &other) {
            return ! (*this == other);
        }

        /// Increment to the next entry in the map.  If we finish the
        /// bin we're in, move on to the next bin (releasing our lock on
        /// the old bin and acquiring a lock on the new bin).  If we
        /// finish the last bin of the map, return the end() iterator.
        void operator++ () {
            DASSERT (m_umc);
            DASSERT (m_bin >= 0);
            ++m_biniterator;
            while (m_biniterator == m_umc->m_bins[m_bin].map.end()) {
                if (m_bin == BINS-1) {
                    // ran off the end
                    unbin();
                    return;
                }
                rebin (m_bin+1);
            }
        }
        void operator++ (int) { ++(*this); }

        /// Lock the bin we point to, if not already locked.
        void lock () {
            if (m_bin >= 0 && !m_locked) {
                m_umc->m_bins[m_bin].lock();
                m_locked = true;
            }
        }
        /// Unlock the bin we point to, if locked.
        void unlock () {
            if (m_bin >= 0 && m_locked) {
                m_umc->m_bins[m_bin].unlock();
                m_locked = false;
            }
        }

        /// Without changing the lock status (i.e., the caller already
        /// holds the lock on the iterator's bin), increment to the next
        /// element within the bin.  Return true if it's pointing to a
        /// valid element afterwards, false if it ran off the end of the
        /// bin contents.
        bool incr_no_lock () {
            ++m_biniterator;
            return (m_biniterator != m_umc->m_bins[m_bin].map.end());
        }

    private:
        // No longer refer to a particular bin, release lock on the bin
        // it had (if any).
        void unbin () {
            if (m_bin >= 0) {
                if (m_locked)
                    unlock ();
                m_bin = -1;
            }
        }

        // Point this iterator to a different bin, releasing locks on
        // the bin it previously referred to.
        void rebin (int newbin) {
            DASSERT (m_umc);
            unbin ();
            m_bin = newbin;
            lock ();
            m_biniterator = m_umc->m_bins[m_bin].map.begin();
        }

        unordered_map_concurrent *m_umc;  // which umc this iterator refers to
        int m_bin;                        // which bin within the umc
        BinMap_iterator_t m_biniterator;  // which entry within the bin
        bool m_locked;                    // do we own the lock on the bin?
    };


    /// Return an interator pointing to the first entry in the map.
    iterator begin () {
        iterator i (this);
        i.rebin (0);
        while (i.m_biniterator == m_bins[i.m_bin].map.end()) {
            if (i.m_bin == BINS-1) {
                // ran off the end
                i.unbin();
                return i;
            }
            i.rebin (i.m_bin+1);
        }
        return i;
    }

    /// Return an iterator signifying the end of the map (no valid
    /// entry pointed to).
    iterator end () {
        iterator i (this);
        return i;
    }

    /// Search for key.  If found, return an iterator referring to the
    /// element, otherwise, return an iterator that is equivalent to
    /// this->end().  If do_lock is true, lock the bin that we're
    /// searching and return the iterator in a locked state, and unlock
    /// the bin again if not found; however, if do_lock is false, assume
    /// that the caller already has the bin locked, so do no locking or
    /// unlocking and return an iterator that is unaware that it holds a
    /// lock.
    iterator find (const KEY &key, bool do_lock = true) {
        size_t b = whichbin(key);
        Bin &bin (m_bins[b]);
        if (do_lock)
            bin.lock ();
        typename BinMap_t::iterator it = bin.map.find (key);
        if (it == bin.map.end()) {
            // not found -- return the 'end' iterator
            if (do_lock)
                bin.unlock();
            return end();
        }
        // Found 
        iterator i (this);
        i.m_bin = (unsigned) b;
        i.m_biniterator = it;
        i.m_locked = do_lock;
        return i;
    }

    /// Search for key. If found, return true and store the value. If not
    /// found, return false and do not alter value. If do_lock is true,
    /// read-lock the bin while we're searching, and release it before
    /// returning; however, if do_lock is false, assume that the caller
    /// already has the bin locked, so do no locking or unlocking.
    bool retrieve (const KEY &key, VALUE &value, bool do_lock = true) {
        size_t b = whichbin(key);
        Bin &bin (m_bins[b]);
        if (do_lock)
            bin.lock ();
        typename BinMap_t::iterator it = bin.map.find (key);
        bool found = (it != bin.map.end());
        if (found)
            value = it->second;
        if (do_lock)
            bin.unlock();
        return found;
    }

    /// Insert <key,value> into the hash map if it's not already there.
    /// Return true if added, false if it was already present.  
    /// If do_lock is true, lock the bin containing key while doing this
    /// operation; if do_lock is false, assume that the caller already
    /// has the bin locked, so do no locking or unlocking.
    bool insert (const KEY &key, const VALUE &value, 
                 bool do_lock = true) {
        size_t b = whichbin(key);
        Bin &bin (m_bins[b]);
        if (do_lock)
            bin.lock ();
        bool add = (bin.map.find (key) == bin.map.end());
        if (add) {
            // not found -- add it!
            bin.map[key] = value;
            ++m_size;
        }
        if (do_lock)
            bin.unlock();
        return add;
    }

    /// If the key is in the map, safely erase it.
    /// If do_lock is true, lock the bin containing key while doing this
    /// operation; if do_lock is false, assume that the caller already
    /// has the bin locked, so do no locking or unlocking.
    void erase (const KEY &key, bool do_lock = true) {
        size_t b = whichbin(key);
        Bin &bin (m_bins[b]);
        if (do_lock)
            bin.lock ();
        typename BinMap_t::iterator it = bin.map.find (key);
        if (it != bin.map.end()) {
            bin.map.erase (it);
        }
        if (do_lock)
            bin.unlock();
    }

    /// Return true if the entire map is empty.
    bool empty() { return m_size == 0; }

    /// Return the total number of entries in the map.
    size_t size () { return size_t(m_size); }

    /// Expliticly lock the bin that will contain the key (regardless of
    /// whether there is such an entry in the map), and return its bin
    /// number.
    size_t lock_bin (const KEY &key) {
        size_t b = whichbin(key);
        m_bins[b].lock ();
        return b;
    }

    /// Explicitly unlock the specified bin (this assumes that the caller
    /// holds the lock).
    void unlock_bin (size_t bin) {
        m_bins[bin].unlock ();
    }

private:
    struct Bin {
        OIIO_CACHE_ALIGN             // align bin to cache line
        mutable spin_mutex mutex;    // mutex for this bin
        BinMap_t map;                // hash map for this bin
#ifndef NDEBUG
        mutable atomic_int m_nlocks; // for debugging
#endif

        Bin () {
#ifndef NDEBUG
            m_nlocks = 0;
#endif
        }
        ~Bin () {
#ifndef NDEBUG
            DASSERT (m_nlocks == 0);
#endif
        }
        void lock () const {
            mutex.lock();
#ifndef NDEBUG
            ++m_nlocks;
            DASSERT_MSG (m_nlocks == 1, "oops, m_nlocks = %d", (int)m_nlocks);
#endif
        }
        void unlock () const {
#ifndef NDEBUG
            DASSERT_MSG (m_nlocks == 1, "oops, m_nlocks = %d", (int)m_nlocks);
            --m_nlocks;
#endif
            mutex.unlock();
        }
    };

    HASH m_hash;         // hashing function
    atomic_int m_size;   // total entries in all bins
    Bin m_bins[BINS];    // the bins

    // Which bin will this key always appear in?
    size_t whichbin (const KEY &key) {
        size_t h = m_hash(key);
        h = (size_t) murmur::fmix (uint64_t(h));  // scramble again
        return h % BINS;
    }

};


OIIO_NAMESPACE_END

#endif // OPENIMAGEIO_UNORDERED_MAP_CONCURRENT_H
