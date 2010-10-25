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

/**
   @file PtexCache.cpp
   @brief LRU Cache Implementation

   <b> Ownership.</b>
   The cache owns all files and data.  If an object is in use, the
   cache will not delete it.  When it is no longer in use, it may be
   kept or may be deleted to keep resource usage under the set limits.
   Deletions are done in lru order.

   <b> Resource Tracking.</b>
   All objects are created as part of the cache and have a ptr back to
   the cache.  Each object updates the cache's resource total when it is
   created or deleted.  Unused objects are kept in an lru list in the
   cache.  Only objects in the lru list can be deleted.
   
   <b> Reference Counting.</b>
   Every object has a ref count to track whether it is being used.
   But objects don't generally ref their parent or children (otherwise
   nothing would get freed).

   A data handle must hold onto (and ref) all the objects it is using
   or may need in the future.  E.g. For a non-tiled face, this is just
   the single face data block.  For a tiled face, the file, the tiled
   face, and the current tile must all be ref'd.

   <b> Parents, Children, and Orphans.</b>
   Every object must be reachable by some other object, generally the
   object that created it, i.e. it's parent.  Even though the parent
   doesn't own it's children (the cache does), it must still track
   them.  Parentless objects (i.e. orphans) are not reachable and are
   not retained in the cache.
   
   When any object is deleted (file, tiled face, etc.), it must orphan
   its children.  If an orphaned child is not in use, then it is
   immediately deleted.  Otherwise, the child's parent ptr is set to
   null and the child is deleted when it is no longer in use.  A
   parent may also orphan a child that it no longer needs; the
   behavior is the same.

   Each object stores a ptr to its own entry within its parent. When
   the object is deleted by the cache, it clears this pointer so that
   the parent no longer sees it.

   <b> Cache LifeTime.</b>
   When a cache is released from its owner, it will delete itself but
   only after all objects it owns are no longer in use.  To do this, a
   ref count on the cache is used.  The owner holds 1 ref (only one
   owner allowed), and each object holds a ref (maintained internally).

   <b> Threading.</b>
   To fully support multi-threading, the following data structures
   must be protected with a mutex: the cache lru lists, ref counts,
   and parent/child ptrs.  This is done with a single mutex per cache.
   To avoid the need for recursive locks and to minimize the number of
   lock points, this mutex is locked and unlocked primarily at the
   external api boundary for methods that affect the cache state:
   (e.g. getMetaData, getData, getTile, release, purge, and purgeAll).
   Care must be taken to release the cache lock when calling any external
   api from within the library.

   Also, in order to prevent thread starvation, the cache lock is
   released during file reads and significant computation such as
   generating an image data reduction.  Additional mutexes are used to
   prevent contention in these cases:
   - 1 mutex per cache to prevent concurrent file opens
   - 1 mutex per file to prevent concurrent file reads
   - 1 mutex per file to prevent concurrent (and possibly redundant)
   reductions.
 */

#include "PtexPlatform.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <ctype.h>
#include "Ptexture.h"
#include "PtexReader.h"
#include "PtexCache.h"

#ifdef GATHER_STATS
namespace PtexInternal {
    CacheStats::~CacheStats() { 
	if (getenv("PTEX_STATS"))
	    print(); 
    }
    
    void CacheStats::print()
    {
	if (nfilesOpened || ndataAllocated || nblocksRead) {
	    printf("Ptex Stats:\n");
	    printf("  nfilesOpened:   %6d\n", nfilesOpened);
	    printf("  nfilesClosed:   %6d\n", nfilesClosed);
	    printf("  ndataAllocated: %6d\n", ndataAllocated);
	    printf("  ndataFreed:     %6d\n", ndataFreed);
	    printf("  nblocksRead:    %6d\n", nblocksRead);
	    printf("  nseeks:         %6d\n", nseeks);
	    if (nblocksRead)
		printf("  avgReadSize:    %6d\n", int(nbytesRead/nblocksRead));
	    if (nseeks)
		printf("  avgSeqReadSize: %6d\n", int(nbytesRead/nseeks));
	    printf("  MbytesRead:     %6.2f\n", nbytesRead/(1024.0*1024.0));
	}
    }

    CacheStats stats;
}
#endif

PtexCacheImpl::~PtexCacheImpl()
{
    // explicitly pop all unused items so that they are 
    // destroyed while cache is still valid
    AutoLockCache locker(cachelock);
    while (_unusedData.pop()) continue;
    while (_unusedFiles.pop()) continue;
}

void PtexCacheImpl::setFileInUse(PtexLruItem* file)
{
    assert(cachelock.locked());
    _unusedFiles.extract(file); 
    _unusedFileCount--;
}

void PtexCacheImpl::setFileUnused(PtexLruItem* file)
{
    assert(cachelock.locked());
    _unusedFiles.push(file);
    _unusedFileCount++;
}

void PtexCacheImpl::removeFile()
{ 
    // cachelock should be locked, but might not be if cache is being deleted
    _unusedFileCount--;
    STATS_INC(nfilesClosed);
}

void PtexCacheImpl::setDataInUse(PtexLruItem* data, int size)
{
    assert(cachelock.locked());
    _unusedData.extract(data); 
    _unusedDataCount--;
    _unusedDataSize -= size;
}

void PtexCacheImpl::setDataUnused(PtexLruItem* data, int size)
{
    assert(cachelock.locked());
    _unusedData.push(data);
    _unusedDataCount++;
    _unusedDataSize += size;
}

void PtexCacheImpl::removeData(int size) {
    // cachelock should be locked, but might not be if cache is being deleted
    _unusedDataCount--;
    _unusedDataSize -= size;
    STATS_INC(ndataFreed);
}


/** Cache for reading Ptex texture files */
class PtexReaderCache : public PtexCacheImpl
{
public:
    PtexReaderCache(int maxFiles, int maxMem, bool premultiply, PtexInputHandler* handler)
	: PtexCacheImpl(maxFiles, maxMem),
	  _io(handler), _cleanupCount(0), _premultiply(premultiply)
    {}

    ~PtexReaderCache()
    {
	// orphan all files since we're about to delete the file table
	// and we don't want the base dtor to try to access it
	purgeAll();
    }

    virtual void setSearchPath(const char* path) 
    {
	// get the open lock since the path is used during open operations
	AutoMutex locker(openlock);
	
	// record path
	_searchpath = path ? path : ""; 

	// split into dirs
	_searchdirs.clear();
	char* buff = strdup(path);
	char* pos = 0;
	char* token = strtok_r(buff, ":", &pos);
	while (token) {
	    if (token[0]) _searchdirs.push_back(token);
	    token = strtok_r(0, ":", &pos);
	}
	free(buff);
    }

    virtual const char* getSearchPath()
    {
	// get the open lock since the path is used during open operations
	AutoMutex locker(openlock);
	return _searchpath.c_str(); 
    }

    virtual PtexTexture* get(const char* path, Ptex::String& error);

    virtual void purge(PtexTexture* texture)
    {
	PtexReader* reader = dynamic_cast<PtexReader*>(texture);
	if (!reader) return;
	purge(reader->path());
    }

    virtual void purge(const char* filename)
    {
	AutoLockCache locker(cachelock); 
	FileMap::iterator iter = _files.find(filename);
	if (iter != _files.end()) {
	    PtexReader* reader = iter->second;
	    if (reader && intptr_t(reader) != -1) {
		reader->orphan();
		iter->second = 0;
	    }
	    _files.erase(iter);
	}
    }

    virtual void purgeAll()
    {
	AutoLockCache locker(cachelock); 
	FileMap::iterator iter = _files.begin();
	while (iter != _files.end()) {
	    PtexReader* reader = iter->second;
	    if (reader && intptr_t(reader) != -1) {
		reader->orphan();
		iter->second = 0;
	    }
	    iter = _files.erase(iter);
	}
    }


    void removeBlankEntries()
    {
	// remove blank file entries to keep map size in check
	for (FileMap::iterator i = _files.begin(); i != _files.end();) {
	    if (i->second == 0) i = _files.erase(i);
	    else i++;
	}
    }


private:
    PtexInputHandler* _io;
    std::string _searchpath;
    std::vector<std::string> _searchdirs;
    typedef PtexDict<PtexReader*> FileMap;
    FileMap _files;
    int _cleanupCount;
    bool _premultiply;
};


PtexTexture* PtexReaderCache::get(const char* filename, Ptex::String& error)
{
    AutoLockCache locker(cachelock); 

    // lookup reader in map
    PtexReader* reader = _files[filename];
    if (reader) {
	// -1 means previous open attempt failed
	if (intptr_t(reader) == -1) return 0;
	reader->ref();
	return reader;
    }
    else {
	bool ok = true;

	// get open lock and make sure we still need to open
	// temporarily release cache lock while we open acquire open lock
	cachelock.unlock();
	AutoMutex openlocker(openlock);
	cachelock.lock();

	// lookup entry again (it might have changed in another thread)
	PtexReader** entry = &_files[filename];

	if (*entry) {
	    // another thread opened it while we were waiting
	    if (intptr_t(*entry) == -1) return 0;
	    (*entry)->ref();
	    return *entry; 
	}
		
	// make a new reader
	reader = new PtexReader((void**)entry, this, _premultiply, _io);

	// temporarily release cache lock while we open the file
	cachelock.unlock();
	std::string tmppath;
	const char* pathToOpen = filename;
	if (!_io) {
            bool isAbsolute = (filename[0] == '/'
#ifdef WINDOWS
                               || filename[0] == '\\'
                               || (isalpha(filename[0]) && filename[1] == ':')
#endif
                               );
	    if (!isAbsolute && !_searchdirs.empty()) {
		// file is relative, search in searchpath
		tmppath.reserve(256); // minimize reallocs (will grow automatically)
		bool found = false;
		struct stat statbuf;
		for (size_t i = 0, size = _searchdirs.size(); i < size; i++) {
		    tmppath = _searchdirs[i];
		    tmppath += "/";
		    tmppath += filename;
		    if (stat(tmppath.c_str(), &statbuf) == 0) {
			found = true;
			pathToOpen = tmppath.c_str();
			break;
		    }
		}
		if (!found) {
		    std::string errstr = "Can't find ptex file: ";
		    errstr += filename;
		    error = errstr.c_str();
		    ok = false;
		}
	    }
	}
	if (ok) ok = reader->open(pathToOpen, error);

	// reacquire cache lock
	cachelock.lock();
	    
	if (!ok) {
	    // open failed, clear parent ptr and unref to delete
	    *entry = reader; // to pass parent check in orphan()
	    reader->orphan();
	    reader->unref();
	    *entry = (PtexReader*)-1; // flag for future lookups
	    return 0;
	}
	    
	// successful open, record in _files map entry
	*entry = reader;

	// clean up unused files
	purgeFiles();

	// Cleanup map every so often so it doesn't get HUGE
	// from being filled with blank entries from dead files.
	// Note: this must be done while we still have the open lock!
	if (++_cleanupCount >= 1000) {
	    _cleanupCount = 0;
	    removeBlankEntries();
	}
    }
    return reader;
}

PtexCache* PtexCache::create(int maxFiles, int maxMem, bool premultiply,
			     PtexInputHandler* handler)
{
    // set default files to 100
    if (maxFiles <= 0) maxFiles = 100;

    // set default memory to 100 MB
    const int MB = 1024*1024;
    if (maxMem <= 0) maxMem = 100 * MB;

    // if memory is < 1 MB, warn
    if (maxMem < 1 * MB) {
	std::cerr << "Warning, PtexCache created with < 1 MB" << std::endl;
    }

    return new PtexReaderCache(maxFiles, maxMem, premultiply, handler);
}


