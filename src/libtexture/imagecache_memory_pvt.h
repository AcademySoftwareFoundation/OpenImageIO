// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
//// Memory tracking utilities specific to the ImageCacheImpl.


#pragma once
#define OPENIMAGEIO_IMAGECACHE_MEMORY_PVT_H

#include <OpenImageIO/memory.h>

#include "imagecache_pvt.h"

OIIO_NAMESPACE_BEGIN

namespace pvt {

// heapsize specialization for ImageCacheFile::LevelInfo
template<>
inline size_t
heapsize<ImageCacheFile::LevelInfo>(const ImageCacheFile::LevelInfo& lvl)
{
    size_t size = heapsize(lvl.polecolor);
    size += heapsize(lvl.m_spec);
    size += heapsize(lvl.nativespec);
    if (lvl.tiles_read) {
        const size_t total_tiles   = lvl.nxtiles * lvl.nytiles * lvl.nztiles;
        const size_t bitfield_size = round_to_multiple(total_tiles, 64) / 64;
        size += sizeof(atomic_ll) * bitfield_size;
    }
    return size;
}

// heapsize specialization for ImageCacheFile::SubimageInfo
template<>
inline size_t
heapsize<ImageCacheFile::SubimageInfo>(const ImageCacheFile::SubimageInfo& sub)
{
    size_t size = heapsize(sub.levels);
    size += heapsize(sub.average_color);
    size += (sub.minwh ? sub.n_mip_levels * sizeof(int) : 0);
    size += (sub.Mlocal ? sizeof(Imath::M44f) : 0);
    return size;
}

// heapsize specialization for ImageCacheFile
template<>
inline size_t
heapsize<ImageCacheFile>(const ImageCacheFile& file)
{
    size_t size = heapsize(file.m_subimages);
    size += heapsize(file.m_configspec);
    size += heapsize(file.m_input);
    size += heapsize(file.m_mipreadcount);
    size += heapsize(file.m_udim_lookup);
    return size;
}

// heapsize specialization for ImageCacheTile
template<>
inline size_t
heapsize<ImageCacheTile>(const ImageCacheTile& tile)
{
    return tile.memsize();
}

// heapsize specialization for ImageCachePerThreadInfo
template<>
inline size_t
heapsize<ImageCachePerThreadInfo>(const ImageCachePerThreadInfo& info)
{
    /// TODO: this should take into account the two last tiles, if their refcount is zero.
    constexpr size_t sizeofPair = sizeof(ustring) + sizeof(ImageCacheFile*);
    return info.m_thread_files.size() * sizeofPair;
}

// heapsize specialization for ImageCacheImpl
template<>
inline size_t
heapsize<ImageCacheImpl>(const ImageCacheImpl& ic)
{
    size_t size = 0;
    // strings
    size += heapsize(ic.m_searchpath) + heapsize(ic.m_plugin_searchpath)
            + heapsize(ic.m_searchdirs);
    // thread info
    size += heapsize(ic.m_all_perthread_info);
    // tile cache
    for (TileCache::iterator t = ic.m_tilecache.begin(),
                             e = ic.m_tilecache.end();
         t != e; ++t)
        size += footprint(t->first) + footprint(t->second);
    // files
    for (FilenameMap::iterator t = ic.m_files.begin(), e = ic.m_files.end();
         t != e; ++t)
        size += footprint(t->first) + footprint(t->second);
    // finger prints; we only account for references, this map does not own the files.
    constexpr size_t sizeofFingerprintPair = sizeof(ustring)
                                             + sizeof(ImageCacheFileRef);
    size += ic.m_fingerprints.size() * sizeofFingerprintPair;
    return size;
}

}  // namespace pvt

OIIO_NAMESPACE_END