#ifndef PtexCache_h
#define PtexCache_h

/* 
PTEX SOFTWARE
Copyright 2009 Disney Enterprises, Inc.  All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
    Studios" or the names of its contributors may NOT be used to
    endorse or promote products derived from this software without
    specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "PtexPlatform.h"
#include <assert.h>

#include "PtexMutex.h"
#include "Ptexture.h"
#include "PtexDict.h"

#define USE_SPIN // use spinlocks instead of mutex for main cache lock

namespace PtexInternal {

#ifdef USE_SPIN
    typedef SpinLock CacheLock;
#else
    typedef Mutex CacheLock;
#endif
    typedef AutoLock<CacheLock> AutoLockCache;

#ifndef NDEBUG
#define GATHER_STATS
#endif

#ifdef GATHER_STATS
    struct CacheStats{
	int nfilesOpened;
	int nfilesClosed;
	int ndataAllocated;
	int ndataFreed;
	int nblocksRead;
	long int nbytesRead;
	int nseeks;

	CacheStats()
	    : nfilesOpened(0),
	      nfilesClosed(0),
	      ndataAllocated(0),
	      ndataFreed(0),
	      nblocksRead(0),
	      nbytesRead(0),
	      nseeks(0)	{}

	~CacheStats();
	void print();
	static void inc(int& val) {
	    static SpinLock spinlock;
	    AutoSpin lock(spinlock);
	    val++;
	}
	static void add(long int& val, int inc) {
	    static SpinLock spinlock;
	    AutoSpin lock(spinlock);
	    val+=inc;
	}
    };
    extern CacheStats stats;
#define STATS_INC(x) stats.inc(stats.x);
#define STATS_ADD(x, y) stats.add(stats.x, y);
#else
#define STATS_INC(x)
#define STATS_ADD(x, y)
#endif
}
using namespace PtexInternal;

/** One item in a cache, typically an open file or a block of memory */
class PtexLruItem {
public:
    bool inuse() { return _prev == 0; }
    void orphan() 
    {
	// parent no longer wants me
	void** p = _parent;
	_parent = 0;
	assert(p && *p == this);
	if (!inuse()) delete this;
	*p = 0;
    }
    template <typename T> static void orphanList(T& list)
    {
	for (typename T::iterator i=list.begin(); i != list.end(); i++) {
	    PtexLruItem* obj = *i;
	    if (obj) {
		assert(obj->_parent == (void**)&*i);
		obj->orphan();
	    }
	}
    }

protected:
    PtexLruItem(void** parent=0)
	: _parent(parent), _prev(0), _next(0) {}
    virtual ~PtexLruItem()
    {
	// detach from parent (if any)
	if (_parent) { assert(*_parent == this); *_parent = 0; }
	// unlink from lru list (if in list)
	if (_prev) {
	    _prev->_next = _next; 
	    _next->_prev = _prev;
	}
    }

private:
    friend class PtexLruList;	// maintains prev/next, deletes
    void** _parent;		// pointer to this item within parent
    PtexLruItem* _prev;		// prev in lru list (0 if in-use)
    PtexLruItem* _next;		// next in lru list (0 if in-use)
};



/** A list of items kept in least-recently-used (LRU) order.
    Only items not in use are kept in the list. */
class PtexLruList {
public:
    PtexLruList() { _end._prev = _end._next = &_end; }
    ~PtexLruList() { while (pop()) continue; }

    void extract(PtexLruItem* node)
    {
	// remove from list
	node->_prev->_next = node->_next;
	node->_next->_prev = node->_prev;
	node->_next = node->_prev = 0;
    }

    void push(PtexLruItem* node)
    {
	// delete node if orphaned
	if (!node->_parent) delete node;
	else {
	    // add to end of list
	    node->_next = &_end;
	    node->_prev = _end._prev;
	    _end._prev->_next = node;
	    _end._prev = node;
	}
    }

    bool pop()
    {
	if (_end._next == &_end) return 0;
	delete _end._next; // item will unlink itself
	return 1;
    }

private:
    PtexLruItem _end;
};


/** Ptex cache implementation.  Maintains a file and memory cache
    within set limits */
class PtexCacheImpl : public PtexCache {
public:
    PtexCacheImpl(int maxFiles, int maxMem)
	: _pendingDelete(false),
	  _maxFiles(maxFiles), _unusedFileCount(0),
	  _maxDataSize(maxMem),
	  _unusedDataSize(0), _unusedDataCount(0)
    {
	/* Allow for a minimum number of data blocks so cache doesn't
	   thrash too much if there are any really big items in the
	   cache pushing over the limit. It's better to go over the
	   limit in this case and make sure there's room for at least
	   a modest number of objects in the cache.
	*/

	// try to allow for at least 10 objects per file (up to 100 files)
	_minDataCount = 10 * maxFiles;
	// but no more than 1000
	if (_minDataCount > 1000) _minDataCount = 1000;
    }

    virtual void release() { delete this; }

    Mutex openlock;
    CacheLock cachelock;

    // internal use - only call from reader classes for deferred deletion
    void setPendingDelete() { _pendingDelete = true; }
    void handlePendingDelete() { if (_pendingDelete) delete this; }

    // internal use - only call from PtexCachedFile, PtexCachedData
    static void addFile() { STATS_INC(nfilesOpened); }
    void setFileInUse(PtexLruItem* file);
    void setFileUnused(PtexLruItem* file);
    void removeFile();
    static void addData() { STATS_INC(ndataAllocated); }
    void setDataInUse(PtexLruItem* data, int size);
    void setDataUnused(PtexLruItem* data, int size);
    void removeData(int size);

    void purgeFiles() {
	while (_unusedFileCount > _maxFiles) 
	{
	    if (!_unusedFiles.pop()) break;
	    // note: pop will destroy item and item destructor will
	    // call removeFile which will decrement _unusedFileCount
	}
    }
    void purgeData() {
	while ((_unusedDataSize > _maxDataSize) &&
	       (_unusedDataCount > _minDataCount))
	{
	    if (!_unusedData.pop()) break;
	    // note: pop will destroy item and item destructor will
	    // call removeData which will decrement _unusedDataSize
	    // and _unusedDataCount
	}
    }

protected:
    ~PtexCacheImpl();

private:
    bool _pendingDelete;	             // flag set if delete is pending

    int _maxFiles, _unusedFileCount;	     // file limit, current unused file count
    long int _maxDataSize, _unusedDataSize;  // data limit (bytes), current size
    int _minDataCount, _unusedDataCount;     // min, current # of unused data blocks
    PtexLruList _unusedFiles, _unusedData;   // lists of unused items
};


/** Cache entry for open file handle */
class PtexCachedFile : public PtexLruItem
{
public:
    PtexCachedFile(void** parent, PtexCacheImpl* cache)
	: PtexLruItem(parent), _cache(cache), _refcount(1)
    { _cache->addFile(); }
    void ref() { assert(_cache->cachelock.locked()); if (!_refcount++) _cache->setFileInUse(this); }
    void unref() { assert(_cache->cachelock.locked()); if (!--_refcount) _cache->setFileUnused(this); }
protected:
    virtual ~PtexCachedFile() {	_cache->removeFile(); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
};


/** Cache entry for allocated memory block */
class PtexCachedData : public PtexLruItem
{
public:
    PtexCachedData(void** parent, PtexCacheImpl* cache, int size)
	: PtexLruItem(parent), _cache(cache), _refcount(1), _size(size)
    { _cache->addData(); }
    void ref() { assert(_cache->cachelock.locked()); if (!_refcount++) _cache->setDataInUse(this, _size); }
    void unref() { assert(_cache->cachelock.locked()); if (!--_refcount) _cache->setDataUnused(this, _size); }
protected:
    void incSize(int size) { _size += size; }
    virtual ~PtexCachedData() { _cache->removeData(_size); }
    PtexCacheImpl* _cache;
private:
    int _refcount;
    int _size;
};


#endif
