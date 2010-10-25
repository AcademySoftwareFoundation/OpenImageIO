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
#include <iostream>
#include <sstream>
#include <errno.h>
#include <stdio.h>

#include "Ptexture.h"
#include "PtexUtils.h"
#include "PtexReader.h"


namespace {
    class DefaultInputHandler : public PtexInputHandler
    {
     public:
	virtual Handle open(const char* path) { return (Handle) fopen(path, "rb"); }
	virtual void seek(Handle handle, int64_t pos) { fseeko((FILE*)handle, pos, SEEK_SET); }
	virtual size_t read(void* buffer, size_t size, Handle handle) {
	    return fread(buffer, size, 1, (FILE*)handle) == 1 ? size : 0;
	}
	virtual bool close(Handle handle) { return fclose((FILE*)handle); }
	virtual const char* lastError() { return strerror(errno); }
    } defaultInputHandler;
}


PtexTexture* PtexTexture::open(const char* path, Ptex::String& error, bool premultiply)
{
    // create a private cache and use it to open the file
    PtexCache* cache = PtexCache::create(1, 1024*1024, premultiply);
    PtexTexture* file = cache->get(path, error);

    // make reader own the cache (so it will delete it later)
    PtexReader* reader = dynamic_cast<PtexReader*> (file);
    if (reader) reader->setOwnsCache();

    // and purge cache so cache doesn't try to hold reader open
    cache->purgeAll();
    return file;
}


bool PtexReader::open(const char* path, Ptex::String& error)
{
    if (!LittleEndian()) {
	error = "Ptex library doesn't currently support big-endian cpu's";
	return 0;
    }
    _path = path;
    _fp = _io->open(path);
    if (!_fp) {
	std::string errstr = "Can't open ptex file: ";
	errstr += path; errstr += "\n"; errstr += _io->lastError();
	error = errstr.c_str();
	return 0;
    }
    readBlock(&_header, HeaderSize);
    if (_header.magic != Magic) {
	std::string errstr = "Not a ptex file: "; errstr += path;
	error = errstr.c_str();
	return 0;
    }
    if (_header.version != 1) {
	char ver[21]; snprintf(ver, 20, "%d", _header.version);
	std::string errstr = "Unsupported ptex file version (";
	errstr += ver; errstr += "): "; errstr += path;
	error = errstr.c_str();
	return 0;
    }
    _pixelsize = _header.pixelSize();

    // read extended header
    memset(&_extheader, 0, sizeof(_extheader));
    readBlock(&_extheader, PtexUtils::min(uint32_t(ExtHeaderSize), _header.extheadersize));

    // compute offsets of various blocks
    FilePos pos = tell();
    _faceinfopos = pos;   pos += _header.faceinfosize;
    _constdatapos = pos;  pos += _header.constdatasize;
    _levelinfopos = pos;  pos += _header.levelinfosize;
    _leveldatapos = pos;  pos += _header.leveldatasize;
    _metadatapos = pos;   pos += _header.metadatazipsize;
                          pos += sizeof(uint64_t); // compatibility barrier
    _lmdheaderpos = pos;  pos += _extheader.lmdheaderzipsize;
    _lmddatapos = pos;    pos += _extheader.lmddatasize;

    // edit data may not start immediately if additional sections have been added
    // use value from extheader if present (and > pos)
    _editdatapos = PtexUtils::max(FilePos(_extheader.editdatapos), pos);

    // read basic file info
    readFaceInfo();
    readConstData();
    readLevelInfo();
    readEditData();

    // an error occurred while reading the file
    if (!_ok) {
	error = _error.c_str();
	return 0;
    }

    return 1;
}


PtexReader::PtexReader(void** parent, PtexCacheImpl* cache, bool premultiply,
		       PtexInputHandler* io)
    : PtexCachedFile(parent, cache),
      _io(io ? io : &defaultInputHandler),
      _premultiply(premultiply),
      _ownsCache(false),
      _ok(true),
      _fp(0),
      _pos(0),
      _pixelsize(0),
      _constdata(0),
      _metadata(0),
      _hasEdits(false)
{
    memset(&_header, 0, sizeof(_header));
    memset(&_zstream, 0, sizeof(_zstream));
    inflateInit(&_zstream);
}


PtexReader::~PtexReader()
{
    if (_fp) _io->close(_fp);

    // we can free the const data directly since we own it (it doesn't go through the cache)
    if (_constdata) free(_constdata);

    // the rest must be orphaned since there may still be references outstanding
    orphanList(_levels);
    for (ReductionMap::iterator i = _reductions.begin(); i != _reductions.end(); i++) {
	FaceData* f = i->second;
	if (f) f->orphan();
    }
    if (_metadata) {
	_metadata->orphan();
	_metadata = 0;
    }

    inflateEnd(&_zstream);

    if (_ownsCache) _cache->setPendingDelete();
}


void PtexReader::release()
{
    PtexCacheImpl* cache = _cache;
    {
	// create local scope for cache lock
	AutoLockCache lock(cache->cachelock);
	unref();
    }
    // If this reader owns the cache, then releasing it may cause deletion of the
    // reader and thus flag the cache for pending deletion.  Call the cache
    // to handle the pending deletion.
    cache->handlePendingDelete();
}


const Ptex::FaceInfo& PtexReader::getFaceInfo(int faceid)
{
    if (faceid >= 0 && uint32_t(faceid) < _faceinfo.size())
	return _faceinfo[faceid];

    static Ptex::FaceInfo dummy;
    return dummy;
}


void PtexReader::readFaceInfo()
{
    if (_faceinfo.empty()) {
	// read compressed face info block
	seek(_faceinfopos);
	int nfaces = _header.nfaces;
	_faceinfo.resize(nfaces);
	readZipBlock(&_faceinfo[0], _header.faceinfosize,
		     sizeof(FaceInfo)*nfaces);

	// generate rfaceids
	_rfaceids.resize(nfaces);
	std::vector<uint32_t> faceids_r(nfaces);
	PtexUtils::genRfaceids(&_faceinfo[0], nfaces,
			       &_rfaceids[0], &faceids_r[0]);

	// store face res values indexed by rfaceid
	_res_r.resize(nfaces);
	for (int i = 0; i < nfaces; i++)
	    _res_r[i] = _faceinfo[faceids_r[i]].res;
    }
}



void PtexReader::readLevelInfo()
{
    if (_levelinfo.empty()) {
	// read level info block
	seek(_levelinfopos);
	_levelinfo.resize(_header.nlevels);
	readBlock(&_levelinfo[0], LevelInfoSize*_header.nlevels);

	// initialize related data
	_levels.resize(_header.nlevels);
	_levelpos.resize(_header.nlevels);
	FilePos pos = _leveldatapos;
	for (int i = 0; i < _header.nlevels; i++) {
	    _levelpos[i] = pos;
	    pos += _levelinfo[i].leveldatasize;
	}
    }
}


void PtexReader::readConstData()
{
    if (!_constdata) {
	// read compressed constant data block
	seek(_constdatapos);
	int size = _pixelsize * _header.nfaces;
	_constdata = (uint8_t*) malloc(size);
	readZipBlock(_constdata, _header.constdatasize, size);
	if (_premultiply && _header.hasAlpha())
	    PtexUtils::multalpha(_constdata, _header.nfaces, _header.datatype,
				 _header.nchannels, _header.alphachan);
    }
}


PtexMetaData* PtexReader::getMetaData()
{
    AutoLockCache locker(_cache->cachelock);
    if (_metadata) _metadata->ref();
    else readMetaData();
    return _metadata;
}


PtexReader::MetaData::Entry*
PtexReader::MetaData::getEntry(const char* key)
{
    MetaMap::iterator iter = _map.find(key);
    if (iter == _map.end()) {
	// not found
	return 0;
    }

    Entry* e = &iter->second;
    if (!e->isLmd) {
	// normal (small) meta data - just return directly
	return e;
    }

    // large meta data - may not be read in yet
    AutoLockCache lock(_cache->cachelock);
    if (e->lmdData) {
	// already in memory, add a ref
	e->lmdData->ref();
	_lmdRefs.push_back(e->lmdData);
	return e;
    }
    else {
	// not present, must read from file
	// temporarily release cache lock so other threads can proceed
	_cache->cachelock.unlock();

	// get read lock and make sure we still need to read
	AutoMutex locker(_reader->readlock);
	if (e->lmdData) {
	    // another thread must have read it while we were waiting
	    _cache->cachelock.lock();

	    // make sure it's still there now that we have the lock
	    if (e->lmdData) {
		e->data = e->lmdData->data();
		_lmdRefs.push_back(e->lmdData);
		e->lmdData->ref();
		return e;
	    }
	}
	// go ahead and read, keep local until finished
	LargeMetaData*& parent = e->lmdData;
	LargeMetaData* volatile lmdData = new LargeMetaData((void**)&parent, _cache, e->datasize);
	e->data = lmdData->data();
	_reader->seek(e->lmdPos);
	_reader->readZipBlock(e->data, e->lmdZipSize, e->datasize);
	// reacquire cache lock and update entry
	_cache->cachelock.lock();
	e->lmdData = lmdData;
	return e;
    }
}


void PtexReader::readMetaData()
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    AutoMutex locker(readlock);
    if (_metadata) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	// make sure it's still there now that we have the lock
	if (_metadata) {
	    _metadata->ref();
	    return;
	}
	_cache->cachelock.unlock();
    }


    // compute total size (including edit blocks) for cache tracking
    int totalsize = _header.metadatamemsize;
    for (size_t i = 0, size = _metaedits.size(); i < size; i++)
	totalsize += _metaedits[i].memsize;

    // allocate new meta data (keep local until fully initialized)
    MetaData* volatile newmeta = new MetaData(&_metadata, _cache, totalsize, this);

    // read primary meta data blocks
    if (_header.metadatamemsize)
	readMetaDataBlock(newmeta, _metadatapos,
			  _header.metadatazipsize, _header.metadatamemsize);

    // read large meta data headers
    if (_extheader.lmdheadermemsize)
	readLargeMetaDataHeaders(newmeta, _lmdheaderpos,
				 _extheader.lmdheaderzipsize, _extheader.lmdheadermemsize);

    // read meta data edits
    for (size_t i = 0, size = _metaedits.size(); i < size; i++)
	readMetaDataBlock(newmeta, _metaedits[i].pos,
			  _metaedits[i].zipsize, _metaedits[i].memsize);

    // store meta data
    _cache->cachelock.lock();
    _metadata = newmeta;

    // clean up unused data
    _cache->purgeData();
}


void PtexReader::readMetaDataBlock(MetaData* metadata, FilePos pos, int zipsize, int memsize)
{
    seek(pos);
    // read from file
    bool useMalloc = memsize > AllocaMax;
    char* buff = useMalloc ? (char*) malloc(memsize) : (char*)alloca(memsize);

    if (readZipBlock(buff, zipsize, memsize)) {
	// unpack data entries
	char* ptr = buff;
	char* end = ptr + memsize;
	while (ptr < end) {
	    uint8_t keysize = *ptr++;
	    char* key = (char*)ptr; ptr += keysize;
	    key[keysize-1] = '\0';
	    uint8_t datatype = *ptr++;
	    uint32_t datasize; memcpy(&datasize, ptr, sizeof(datasize));
	    ptr += sizeof(datasize);
	    char* data = ptr; ptr += datasize;
	    metadata->addEntry(keysize-1, key, datatype, datasize, data);
	}
    }
    if (useMalloc) free(buff);
}


void PtexReader::readLargeMetaDataHeaders(MetaData* metadata, FilePos pos, int zipsize, int memsize)
{
    seek(pos);
    // read from file
    bool useMalloc = memsize > AllocaMax;
    char* buff = useMalloc ? (char*) malloc(memsize) : (char*)alloca(memsize);

    if (readZipBlock(buff, zipsize, memsize)) {
	pos += zipsize;

	// unpack data entries
	char* ptr = buff;
	char* end = ptr + memsize;
	while (ptr < end) {
	    uint8_t keysize = *ptr++;
	    char* key = (char*)ptr; ptr += keysize;
	    uint8_t datatype = *ptr++;
	    uint32_t datasize; memcpy(&datasize, ptr, sizeof(datasize));
	    ptr += sizeof(datasize);
	    uint32_t zipsize; memcpy(&zipsize, ptr, sizeof(zipsize));
	    ptr += sizeof(zipsize);
	    metadata->addLmdEntry(keysize-1, key, datatype, datasize, pos, zipsize);
	    pos += zipsize;
	}
    }
    if (useMalloc) free(buff);
}

void PtexReader::readEditData()
{
    // determine file range to scan for edits
    FilePos pos = FilePos(_editdatapos), endpos;
    if (_extheader.editdatapos > 0) {
	// newer files record the edit data position and size in the extheader
	// note: position will be non-zero even if size is zero
	endpos = FilePos(pos + _extheader.editdatasize);
    }
    else {
	// must have an older file, just read until EOF
	endpos = FilePos((uint64_t)-1);
    }

    while (pos < endpos) {
	seek(pos);
	// read the edit data header
	uint8_t edittype = et_editmetadata;
	uint32_t editsize;
	if (!readBlock(&edittype, sizeof(edittype), /*reporterror*/ false)) break;
	if (!readBlock(&editsize, sizeof(editsize), /*reporterror*/ false)) break;
	if (!editsize) break;
	_hasEdits = true;
	pos = tell() + editsize;
	switch (edittype) {
	case et_editfacedata:   readEditFaceData(); break;
	case et_editmetadata:   readEditMetaData(); break;
	}
    }
}


void PtexReader::readEditFaceData()
{
    // read header
    EditFaceDataHeader efdh;
    if (!readBlock(&efdh, EditFaceDataHeaderSize)) return;

    // update face info
    int faceid = efdh.faceid;
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return;
    FaceInfo& f = _faceinfo[faceid];
    f = efdh.faceinfo;
    f.flags |= FaceInfo::flag_hasedits;

    // read const value now
    uint8_t* constdata = _constdata + _pixelsize * faceid;
    if (!readBlock(constdata, _pixelsize)) return;
    if (_premultiply && _header.hasAlpha())
	PtexUtils::multalpha(constdata, 1, _header.datatype,
			     _header.nchannels, _header.alphachan);

    // update header info for remaining data
    if (!f.isConstant()) {
	_faceedits.push_back(FaceEdit());
	FaceEdit& e = _faceedits.back();
	e.pos = tell();
	e.faceid = faceid;
	e.fdh = efdh.fdh;
    }
}


void PtexReader::readEditMetaData()
{
    // read header
    EditMetaDataHeader emdh;
    if (!readBlock(&emdh, EditMetaDataHeaderSize)) return;

    // record header info for later
    _metaedits.push_back(MetaEdit());
    MetaEdit& e = _metaedits.back();
    e.pos = tell();
    e.zipsize = emdh.metadatazipsize;
    e.memsize = emdh.metadatamemsize;
}


bool PtexReader::readBlock(void* data, int size, bool reporterror)
{
    int result = _io->read(data, size, _fp);
    if (result == size) {
	_pos += size;
	STATS_INC(nblocksRead);
	STATS_ADD(nbytesRead, size);
	return 1;
    }
    if (reporterror)
	setError("PtexReader error: read failed (EOF)");
    return 0;
}


bool PtexReader::readZipBlock(void* data, int zipsize, int unzipsize)
{
    void* buff = alloca(BlockSize);
    _zstream.next_out = (Bytef*) data;
    _zstream.avail_out = unzipsize;

    while (1) {
	int size = (zipsize < BlockSize) ? zipsize : BlockSize;
	zipsize -= size;
	if (!readBlock(buff, size)) break;
	_zstream.next_in = (Bytef*) buff;
	_zstream.avail_in = size;
	int zresult = inflate(&_zstream, zipsize ? Z_NO_FLUSH : Z_FINISH);
	if (zresult == Z_STREAM_END) break;
	if (zresult != Z_OK) {
	    setError("PtexReader error: unzip failed, file corrupt");
	    inflateReset(&_zstream);
	    return 0;
	}
    }

    int total = _zstream.total_out;
    inflateReset(&_zstream);
    return total == unzipsize;
}


void PtexReader::readLevel(int levelid, Level*& level)
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    AutoMutex locker(readlock);
    if (level) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	// make sure it's still there now that we have the lock
	if (level) {
	    level->ref();
	    return;
	}
	_cache->cachelock.unlock();
    }

    // go ahead and read the level
    LevelInfo& l = _levelinfo[levelid];

    // keep new level local until finished
    Level* volatile newlevel = new Level((void**)&level, _cache, l.nfaces);
    seek(_levelpos[levelid]);
    readZipBlock(&newlevel->fdh[0], l.levelheadersize, FaceDataHeaderSize * l.nfaces);
    computeOffsets(tell(), l.nfaces, &newlevel->fdh[0], &newlevel->offsets[0]);

    // apply edits (if any) to level 0
    if (levelid == 0) {
	for (size_t i = 0, size = _faceedits.size(); i < size; i++) {
	    FaceEdit& e = _faceedits[i];
	    newlevel->fdh[e.faceid] = e.fdh;
	    newlevel->offsets[e.faceid] = e.pos;
	}
    }

    // don't assign to result until level data is fully initialized
    _cache->cachelock.lock();
    level = newlevel;

    // clean up unused data
    _cache->purgeData();
}


void PtexReader::readFace(int levelid, Level* level, int faceid)
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    FaceData*& face = level->faces[faceid];
    AutoMutex locker(readlock);

    if (face) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	// make sure it's still there now that we have the lock
	if (face) {
	    face->ref();
	    return;
	}
	_cache->cachelock.unlock();
    }

    // Go ahead and read the face, and read nearby faces if
    // possible. The goal is to coalesce small faces into single
    // runs of consecutive reads to minimize seeking and take
    // advantage of read-ahead buffering.

    // Try to read as many faces as will fit in BlockSize.  Use the
    // in-memory size rather than the on-disk size to prevent flooding
    // the memory cache.  And don't coalesce w/ tiled faces as these
    // are meant to be read individually.

    // scan both backwards and forwards looking for unread faces
    int first = faceid, last = faceid;
    int totalsize = 0;

    FaceDataHeader fdh = level->fdh[faceid];
    if (fdh.encoding() != enc_tiled) {
	totalsize += unpackedSize(fdh, levelid, faceid);

	int nfaces = int(level->fdh.size());
	while (1) {
	    int f = first-1;
	    if (f < 0 || level->faces[f]) break;
	    fdh = level->fdh[f];
	    if (fdh.encoding() == enc_tiled) break;
	    int size = totalsize + unpackedSize(fdh, levelid, f);
	    if (size > BlockSize) break;
	    first = f;
	    totalsize = size;
	}
	while (1) {
	    int f = last+1;
	    if (f >= nfaces || level->faces[f]) break;
	    fdh = level->fdh[f];
	    if (fdh.encoding() == enc_tiled) break;
	    int size = totalsize + unpackedSize(fdh, levelid, f);
	    if (size > BlockSize) break;
	    last = f;
	    totalsize = size;
	}
    }

    // read all faces in range
    // keep track of extra faces we read so we can add them to the cache later
    std::vector<FaceData*> extraFaces;
    extraFaces.reserve(last-first);

    for (int i = first; i <= last; i++) {
	fdh = level->fdh[i];
	// skip faces with zero size (which is true for level-0 constant faces)
	if (fdh.blocksize()) {
	    FaceData*& face = level->faces[i];
	    readFaceData(level->offsets[i], fdh, getRes(levelid, i), levelid, face);
	    if (i != faceid) extraFaces.push_back(face);
	}
    }

    // reacquire cache lock, then unref extra faces to add them to the cache
    _cache->cachelock.lock();
    for (size_t i = 0, size = extraFaces.size(); i < size; i++)
	extraFaces[i]->unref();
}


void PtexReader::TiledFace::readTile(int tile, FaceData*& data)
{
    // temporarily release cache lock so other threads can proceed
    _cache->cachelock.unlock();

    // get read lock and make sure we still need to read
    AutoMutex locker(_reader->readlock);
    if (data) {
	// another thread must have read it while we were waiting
	_cache->cachelock.lock();
	// make sure it's still there now that we have the lock
	if (data) {
	    data->ref();
	    return;
	}
	_cache->cachelock.unlock();
    }

    // go ahead and read the face data
    _reader->readFaceData(_offsets[tile], _fdh[tile], _tileres, _levelid, data);
    _cache->cachelock.lock();

    // clean up unused data
    _cache->purgeData();
}


void PtexReader::readFaceData(FilePos pos, FaceDataHeader fdh, Res res, int levelid,
			      FaceData*& face)
{
    // keep new face local until fully initialized
    FaceData* volatile newface = 0;

    seek(pos);
    switch (fdh.encoding()) {
    case enc_constant:
	{
	    ConstantFace* pf = new ConstantFace((void**)&face, _cache, _pixelsize);
	    readBlock(pf->data(), _pixelsize);
	    if (levelid==0 && _premultiply && _header.hasAlpha())
		PtexUtils::multalpha(pf->data(), 1, _header.datatype,
				     _header.nchannels, _header.alphachan);
	    newface = pf;
	}
	break;
    case enc_tiled:
	{
	    Res tileres;
	    readBlock(&tileres, sizeof(tileres));
	    uint32_t tileheadersize;
	    readBlock(&tileheadersize, sizeof(tileheadersize));
	    TiledFace* tf = new TiledFace((void**)&face, _cache, res, tileres, levelid, this);
	    readZipBlock(&tf->_fdh[0], tileheadersize, FaceDataHeaderSize * tf->_ntiles);
	    computeOffsets(tell(), tf->_ntiles, &tf->_fdh[0], &tf->_offsets[0]);
	    newface = tf;
	}
	break;
    case enc_zipped:
    case enc_diffzipped:
	{
	    int uw = res.u(), vw = res.v();
	    int npixels = uw * vw;
	    int unpackedSize = _pixelsize * npixels;
	    PackedFace* pf = new PackedFace((void**)&face, _cache,
					    res, _pixelsize, unpackedSize);
	    void* tmp = alloca(unpackedSize);
	    readZipBlock(tmp, fdh.blocksize(), unpackedSize);
	    if (fdh.encoding() == enc_diffzipped)
		PtexUtils::decodeDifference(tmp, unpackedSize, _header.datatype);
	    PtexUtils::interleave(tmp, uw * DataSize(_header.datatype), uw, vw,
				  pf->data(), uw * _pixelsize,
				  _header.datatype, _header.nchannels);
	    if (levelid==0 && _premultiply && _header.hasAlpha())
		PtexUtils::multalpha(pf->data(), npixels, _header.datatype,
				     _header.nchannels, _header.alphachan);
	    newface = pf;
	}
	break;
    }

    face = newface;
}


void PtexReader::getData(int faceid, void* buffer, int stride)
{
    if (!_ok) return;
    FaceInfo& f = _faceinfo[faceid];
    getData(faceid, buffer, stride, f.res);
}


void PtexReader::getData(int faceid, void* buffer, int stride, Res res)
{
    if (!_ok) return;

    // note - all locking is handled in called getData methods
    int resu = res.u(), resv = res.v();
    int rowlen = _pixelsize * resu;
    if (stride == 0) stride = rowlen;

    PtexPtr<PtexFaceData> d ( getData(faceid, res) );
    if (!d) return;
    if (d->isConstant()) {
	// fill dest buffer with pixel value
	PtexUtils::fill(d->getData(), buffer, stride,
			resu, resv, _pixelsize);
    }
    else if (d->isTiled()) {
	// loop over tiles
	Res tileres = d->tileRes();
	int ntilesu = res.ntilesu(tileres);
	int ntilesv = res.ntilesv(tileres);
	int tileures = tileres.u();
	int tilevres = tileres.v();
	int tilerowlen = _pixelsize * tileures;
	int tile = 0;
	char* dsttilerow = (char*) buffer;
	for (int i = 0; i < ntilesv; i++) {
	    char* dsttile = dsttilerow;
	    for (int j = 0; j < ntilesu; j++) {
		PtexPtr<PtexFaceData> t ( d->getTile(tile++) );
		if (!t) { i = ntilesv; break; }
		if (t->isConstant())
		    PtexUtils::fill(t->getData(), dsttile, stride,
				    tileures, tilevres, _pixelsize);
		else
		    PtexUtils::copy(t->getData(), tilerowlen, dsttile, stride,
				    tilevres, tilerowlen);
		dsttile += tilerowlen;
	    }
	    dsttilerow += stride * tilevres;
	}
    }
    else {
	PtexUtils::copy(d->getData(), rowlen, buffer, stride, resv, rowlen);
    }
}


PtexFaceData* PtexReader::getData(int faceid)
{
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;
    if (!_ok) return 0;

    FaceInfo& fi = _faceinfo[faceid];
    if (fi.isConstant() || fi.res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize, _pixelsize);
    }

    // get level zero (full) res face
    AutoLockCache locker(_cache->cachelock);
    Level* level = getLevel(0);
    FaceData* face = getFace(0, level, faceid);
    level->unref();
    return face;
}


PtexFaceData* PtexReader::getData(int faceid, Res res)
{
    if (!_ok) return 0;
    if (faceid < 0 || size_t(faceid) >= _header.nfaces) return 0;

    FaceInfo& fi = _faceinfo[faceid];
    if ((fi.isConstant() && res >= 0) || res == 0) {
	return new ConstDataPtr(getConstData() + faceid * _pixelsize, _pixelsize);
    }

    // lock cache (can't autolock since we might need to unlock early)
    _cache->cachelock.lock();


    // determine how many reduction levels are needed
    int redu = fi.res.ulog2 - res.ulog2, redv = fi.res.vlog2 - res.vlog2;

    if (redu == 0 && redv == 0) {
	// no reduction - get level zero (full) res face
	Level* level = getLevel(0);
	FaceData* face = getFace(0, level, faceid);
	level->unref();
	_cache->cachelock.unlock();
	return face;
    }

    if (redu == redv && !fi.hasEdits() && res >= 0) {
	// reduction is symmetric and non-negative
	// and face has no edits => access data from reduction level (if present)
	int levelid = redu;
	if (size_t(levelid) < _levels.size()) {
	    Level* level = getLevel(levelid);

	    // get reduction face id
	    int rfaceid = _rfaceids[faceid];

	    // get the face data (if present)
	    FaceData* face = 0;
	    if (size_t(rfaceid) < level->faces.size())
		face = getFace(levelid, level, rfaceid);
	    level->unref();
	    if (face) {
		_cache->cachelock.unlock();
		return face;
	    }
	}
    }

    // dynamic reduction required - look in dynamic reduction cache
    FaceData*& face = _reductions[ReductionKey(faceid, res)];
    if (face) {
	face->ref();
	_cache->cachelock.unlock();
	return face;
    }

    // not found,  generate new reduction
    // unlock cache - getData and reduce will handle their own locking
    _cache->cachelock.unlock();

    if (res.ulog2 < 0 || res.vlog2 < 0) {
	std::cerr << "PtexReader::getData - reductions below 1 pixel not supported" << std::endl;
	return 0;
    }
    if (redu < 0 || redv < 0) {
	std::cerr << "PtexReader::getData - enlargements not supported" << std::endl;
	return 0;
    }
    if (_header.meshtype == mt_triangle)
    {
	if (redu != redv) {
	    std::cerr << "PtexReader::getData - anisotropic reductions not supported for triangle mesh" << std::endl;
	    return 0;
	}
	PtexPtr<PtexFaceData> psrc ( getData(faceid, Res(res.ulog2+1, res.vlog2+1)) );
	FaceData* src = dynamic_cast<FaceData*>(psrc.get());
	assert(src);
	if (src) src->reduce(face, this, res, PtexUtils::reduceTri);
	return face;
    }

    // determine which direction to blend
    bool blendu;
    if (redu == redv) {
	// for symmetric face blends, alternate u and v blending
	blendu = (res.ulog2 & 1);
    }
    else blendu = redu > redv;

    if (blendu) {
	// get next-higher u-res and reduce in u
	PtexPtr<PtexFaceData> psrc ( getData(faceid, Res(res.ulog2+1, res.vlog2)) );
	FaceData* src = dynamic_cast<FaceData*>(psrc.get());
	assert(src);
	if (src) src->reduce(face, this, res, PtexUtils::reduceu);
    }
    else {
	// get next-higher v-res and reduce in v
	PtexPtr<PtexFaceData> psrc ( getData(faceid, Res(res.ulog2, res.vlog2+1)) );
	FaceData* src = dynamic_cast<FaceData*>(psrc.get());
	assert(src);
	if (src) src->reduce(face, this, res, PtexUtils::reducev);
    }

    return face;
}


void PtexReader::blendFaces(FaceData*& face, int faceid, Res res, bool blendu)
{
    Res pres;   // parent res, 1 higher in blend direction
    int length; // length of blend edge (1xN or Nx1)
    int e1, e2; // neighboring edge ids
    if (blendu) {
	assert(res.ulog2 < 0); // res >= 0 requires reduction, not blending
	length = (res.vlog2 <= 0 ? 1 : res.v());
	e1 = e_bottom; e2 = e_top;
	pres = Res(res.ulog2+1, res.vlog2);
    }
    else {
	assert(res.vlog2 < 0);
	length = (res.ulog2 <= 0 ? 1 : res.u());
	e1 = e_right; e2 = e_left;
	pres = Res(res.ulog2, res.vlog2+1);
    }

    // get neighbor face ids
    FaceInfo& f = _faceinfo[faceid];
    int nf1 = f.adjfaces[e1], nf2 = f.adjfaces[e2];

    // compute rotation of faces relative to current
    int r1 = (f.adjedge(e1)-e1+2)&3;
    int r2 = (f.adjedge(e2)-e2+2)&3;

    // swap u and v res for faces rotated +/- 90 degrees
    Res pres1 = pres, pres2 = pres;
    if (r1 & 1) pres1.swapuv();
    if (r2 & 1) pres2.swapuv();

    // ignore faces that have insufficient res (unlikely, but possible)
    if (nf1 >= 0 && !(_faceinfo[nf1].res >= pres)) nf1 = -1;
    if (nf2 >= 0 && !(_faceinfo[nf2].res >= pres)) nf2 = -1;

    // get parent face data
    int nf = 1;			// number of faces to blend (1 to 3)
    bool flip[3];		// true if long dimension needs to be flipped
    PtexFaceData* psrc[3];	// the face data
    psrc[0] = getData(faceid, pres);
    flip[0] = 0;		// don't flip main face
    if (nf1 >= 0) {
	// face must be flipped if rot is 1 or 2 for blendu, or 2 or 3 for blendv
	// thus, just add the blendu bool val to align the ranges and check bit 1
	// also, no need to flip if length is zero
	flip[nf] = length ? (r1 + blendu) & 1 : 0;
	psrc[nf++] = getData(nf1, pres1);
    }
    if (nf2 >= 0) {
	flip[nf] = length ? (r2 + blendu) & 1 : 0;
	psrc[nf++] = getData(nf2, pres2);
    }

    // get reduce lock and make sure we still need to reduce
    AutoMutex rlocker(reducelock);
    if (face) {
	// another thread must have generated it while we were waiting
	AutoLockCache locker(_cache->cachelock);
	// make sure it's still there now that we have the lock
	if (face) {
	    face->ref();
	    // release parent data
	    for (int i = 0; i < nf; i++) psrc[i]->release();
	    return;
	}
    }

    // allocate a new face data (1 x N or N x 1)
    DataType dt = datatype();
    int nchan = nchannels();
    int size = _pixelsize * length;
    PackedFace* pf = new PackedFace((void**)&face, _cache, res,
				    _pixelsize, size);
    void* data = pf->getData();
    if (nf == 1) {
	// no neighbors - just copy face
	memcpy(data, psrc[0]->getData(), size);
    }
    else {
	float weight = 1.0f / nf;
	memset(data, 0, size);
	for (int i = 0; i < nf; i++)
	    PtexUtils::blend(psrc[i]->getData(), weight, data, flip[i],
			     length, dt, nchan);
    }

    {
	AutoLockCache clocker(_cache->cachelock);
	face = pf;

	// clean up unused data
	_cache->purgeData();
    }

    // release parent data
    for (int i = 0; i < nf; i++) psrc[i]->release();
}


void PtexReader::getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels)
{
    memset(result, 0, nchannels);

    // clip nchannels against actual number available
    nchannels = PtexUtils::min(nchannels,
			       _header.nchannels-firstchan);
    if (nchannels <= 0) return;

    // get raw pixel data
    PtexPtr<PtexFaceData> data ( getData(faceid) );
    if (!data) return;
    void* pixel = alloca(_pixelsize);
    data->getPixel(u, v, pixel);

    // adjust for firstchan offset
    int datasize = DataSize(_header.datatype);
    if (firstchan)
	pixel = (char*) pixel + datasize * firstchan;

    // convert/copy to result as needed
    if (_header.datatype == dt_float)
	memcpy(result, pixel, datasize * nchannels);
    else
	ConvertToFloat(result, pixel, _header.datatype, nchannels);
}


void PtexReader::getPixel(int faceid, int u, int v,
			  float* result, int firstchan, int nchannels,
			  Ptex::Res res)
{
    memset(result, 0, nchannels);

    // clip nchannels against actual number available
    nchannels = PtexUtils::min(nchannels,
			       _header.nchannels-firstchan);
    if (nchannels <= 0) return;

    // get raw pixel data
    PtexPtr<PtexFaceData> data ( getData(faceid, res) );
    if (!data) return;
    void* pixel = alloca(_pixelsize);
    data->getPixel(u, v, pixel);

    // adjust for firstchan offset
    int datasize = DataSize(_header.datatype);
    if (firstchan)
	pixel = (char*) pixel + datasize * firstchan;

    // convert/copy to result as needed
    if (_header.datatype == dt_float)
	memcpy(result, pixel, datasize * nchannels);
    else
	ConvertToFloat(result, pixel, _header.datatype, nchannels);
}


void PtexReader::PackedFace::reduce(FaceData*& face, PtexReader* r,
				    Res newres, PtexUtils::ReduceFn reducefn)
{
    // get reduce lock and make sure we still need to reduce
    AutoMutex rlocker(r->reducelock);
    if (face) {
	// another thread must have generated it while we were waiting
	AutoLockCache clocker(_cache->cachelock);
	// make sure it's still there now that we have the lock
	if (face) {
	    face->ref();
	    return;
	}
    }

    // allocate a new face and reduce image
    DataType dt = r->datatype();
    int nchan = r->nchannels();
    PackedFace* pf = new PackedFace((void**)&face, _cache, newres,
				    _pixelsize, _pixelsize * newres.size());
    // reduce and copy into new face
    reducefn(_data, _pixelsize * _res.u(), _res.u(), _res.v(),
	     pf->_data, _pixelsize * newres.u(), dt, nchan);
    AutoLockCache clocker(_cache->cachelock);
    face = pf;

    // clean up unused data
    _cache->purgeData();
}



void PtexReader::ConstantFace::reduce(FaceData*& face, PtexReader*,
				      Res, PtexUtils::ReduceFn)
{
    // get cache lock (just to protect the ref count)
    AutoLockCache locker(_cache->cachelock);

    // must make a new constant face (even though it's identical to this one)
    // because it will be owned by a different reduction level
    // and will therefore have a different parent
    ConstantFace* pf = new ConstantFace((void**)&face, _cache, _pixelsize);
    memcpy(pf->_data, _data, _pixelsize);
    face = pf;
}


void PtexReader::TiledFaceBase::reduce(FaceData*& face, PtexReader* r,
				       Res newres, PtexUtils::ReduceFn reducefn)
{
    // get reduce lock and make sure we still need to reduce
    AutoMutex rlocker(r->reducelock);
    if (face) {
	// another thread must have generated it while we were waiting
	AutoLockCache clocker(_cache->cachelock);
	// make sure it's still there now that we have the lock
	if (face) {
	    face->ref();
	    return;
	}
    }

    /* Tiled reductions should generally only be anisotropic (just u
       or v, not both) since isotropic reductions are precomputed and
       stored on disk.  (This function should still work for isotropic
       reductions though.)

       In the anisotropic case, the number of tiles should be kept the
       same along the direction not being reduced in order to preserve
       the laziness of the file access.  In contrast, if reductions
       were not tiled, then any reduction would read all the tiles and
       defeat the purpose of tiling.
    */

    // keep new face local until fully initialized
    FaceData* volatile newface = 0;

    // don't tile if either dimension is 1 (rare, would complicate blendFaces too much)
    Res newtileres;
    if (newres.ulog2 == 1 || newres.vlog2 == 1) {
	newtileres = newres;
    }
    else {
	// propagate the tile res to the reduction
	newtileres = _tileres;
	// but make sure tile isn't larger than the new face!
	if (newtileres.ulog2 > newres.ulog2) newtileres.ulog2 = newres.ulog2;
	if (newtileres.vlog2 > newres.vlog2) newtileres.vlog2 = newres.vlog2;
    }


    // determine how many tiles we will have on the reduction
    int newntiles = newres.ntiles(newtileres);

    if (newntiles == 1) {
	// no need to keep tiling, reduce tiles into a single face
	// first, get all tiles and check if they are constant (with the same value)
	PtexFaceData** tiles = (PtexFaceData**) alloca(_ntiles * sizeof(PtexFaceData*));
	bool allConstant = true;
	for (int i = 0; i < _ntiles; i++) {
	    PtexFaceData* tile = tiles[i] = getTile(i);
	    allConstant = (allConstant && tile->isConstant() &&
			   (i == 0 || (0 == memcmp(tiles[0]->getData(), tile->getData(),
						   _pixelsize))));
	}
	if (allConstant) {
	    // allocate a new constant face
	    newface = new ConstantFace((void**)&face, _cache, _pixelsize);
	    memcpy(newface->getData(), tiles[0]->getData(), _pixelsize);
	}
	else {
	    // allocate a new packed face
	    newface = new PackedFace((void**)&face, _cache, newres,
				     _pixelsize, _pixelsize*newres.size());

	    int tileures = _tileres.u();
	    int tilevres = _tileres.v();
	    int sstride = _pixelsize * tileures;
	    int dstride = _pixelsize * newres.u();
	    int dstepu = dstride/_ntilesu;
	    int dstepv = dstride*newres.v()/_ntilesv - dstepu*(_ntilesu-1);

	    char* dst = (char*) newface->getData();
	    for (int i = 0; i < _ntiles;) {
		PtexFaceData* tile = tiles[i];
		if (tile->isConstant())
		    PtexUtils::fill(tile->getData(), dst, dstride,
				    newres.u()/_ntilesu, newres.v()/_ntilesv,
				    _pixelsize);
		else
		    reducefn(tile->getData(), sstride, tileures, tilevres,
			     dst, dstride, _dt, _nchan);
		i++;
		dst += i%_ntilesu ? dstepu : dstepv;
	    }
	}
	// release the tiles
	for (int i = 0; i < _ntiles; i++) tiles[i]->release();
    }
    else {
	// otherwise, tile the reduced face
	newface = new TiledReducedFace((void**)&face, _cache, newres, newtileres,
				       _dt, _nchan, this, reducefn);
    }
    AutoLockCache clocker(_cache->cachelock);
    face = newface;

    // clean up unused data
    _cache->purgeData();
}


void PtexReader::TiledFaceBase::getPixel(int ui, int vi, void* result)
{
    int tileu = ui >> _tileres.ulog2;
    int tilev = vi >> _tileres.vlog2;
    PtexPtr<PtexFaceData> tile ( getTile(tilev * _ntilesu + tileu) );
    tile->getPixel(ui - (tileu<<_tileres.ulog2),
		   vi - (tilev<<_tileres.vlog2), result);
}



PtexFaceData* PtexReader::TiledReducedFace::getTile(int tile)
{
    _cache->cachelock.lock();
    FaceData*& face = _tiles[tile];
    if (face) {
	face->ref();
	_cache->cachelock.unlock();
	return face;
    }

    // first, get all parent tiles for this tile
    // and check if they are constant (with the same value)
    int pntilesu = _parentface->ntilesu();
    int pntilesv = _parentface->ntilesv();
    int nu = pntilesu / _ntilesu; // num parent tiles for this tile in u dir
    int nv = pntilesv / _ntilesv; // num parent tiles for this tile in v dir

    int ntiles = nu*nv; // num parent tiles for this tile
    PtexFaceData** tiles = (PtexFaceData**) alloca(ntiles * sizeof(PtexFaceData*));
    bool allConstant = true;
    int ptile = (tile/_ntilesu) * nv * pntilesu + (tile%_ntilesu) * nu;
    for (int i = 0; i < ntiles;) {
	// temporarily release cache lock while we get parent tile
	_cache->cachelock.unlock();
	PtexFaceData* tile = tiles[i] = _parentface->getTile(ptile);
	_cache->cachelock.lock();
	allConstant = (allConstant && tile->isConstant() &&
		       (i==0 || (0 == memcmp(tiles[0]->getData(), tile->getData(),
					     _pixelsize))));
	i++;
	ptile += i%nu? 1 : pntilesu - nu + 1;
    }

    // make sure another thread didn't make the tile while we were checking
    if (face) {
	face->ref();
	_cache->cachelock.unlock();
	// release the tiles
	for (int i = 0; i < ntiles; i++) tiles[i]->release();
	return face;
    }

    if (allConstant) {
	// allocate a new constant face
	face = new ConstantFace((void**)&face, _cache, _pixelsize);
	memcpy(face->getData(), tiles[0]->getData(), _pixelsize);
    }
    else {
	// allocate a new packed face for the tile
	face = new PackedFace((void**)&face, _cache, _tileres,
			      _pixelsize, _pixelsize*_tileres.size());

	// generate reduction from parent tiles
	int ptileures = _parentface->tileres().u();
	int ptilevres = _parentface->tileres().v();
	int sstride = ptileures * _pixelsize;
	int dstride = _tileres.u() * _pixelsize;
	int dstepu = dstride/nu;
	int dstepv = dstride*_tileres.v()/nv - dstepu*(nu-1);

	char* dst = (char*) face->getData();
	for (int i = 0; i < ntiles;) {
	    PtexFaceData* tile = tiles[i];
	    if (tile->isConstant())
		PtexUtils::fill(tile->getData(), dst, dstride,
				_tileres.u()/nu, _tileres.v()/nv,
				_pixelsize);
	    else
		_reducefn(tile->getData(), sstride, ptileures, ptilevres,
			  dst, dstride, _dt, _nchan);
	    i++;
	    dst += i%nu ? dstepu : dstepv;
	}
    }

    _cache->cachelock.unlock();

    // release the tiles
    for (int i = 0; i < ntiles; i++) tiles[i]->release();
    return face;
}
