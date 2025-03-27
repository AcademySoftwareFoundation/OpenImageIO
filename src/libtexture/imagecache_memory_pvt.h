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
    return file.heapsize();
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
    return info.heapsize();
}

// heapsize specialization for ImageCacheImpl
template<>
inline size_t
heapsize<ImageCacheImpl>(const ImageCacheImpl& ic)
{
    return ic.heapsize();
}

}  // namespace pvt

OIIO_NAMESPACE_END
