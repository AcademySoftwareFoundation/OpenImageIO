// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
//// Memory printing utilities specific to the ImageCacheImpl.


#pragma once
#define OPENIMAGEIO_IMAGECACHE_MEMORY_PRINT_H

#include "imagecache_memory_pvt.h"

#include <sstream>

OIIO_NAMESPACE_BEGIN

namespace pvt {

//// Memory tracking helper to get ImageCacheImpl statistics

//! recorded entries per file format
enum FileFootprintEnty : uint8_t {
    kMem = 0,
    kCount,
    kSpecMem,
    kSpecCount,
    kInputMem,
    kInputCount,
    kSubImageMem,
    kSubImageCount,
    kLevelInfoMem,
    kLevelInfoCount,
    kLevelInfoSpecMem,
    kLevelInfoSpecMembMem,
    kLevelInfoSpecParmsMem,
    kLevelInfoSpecChanMem,
    kFootprintEntrySize
};

typedef std::array<size_t, FileFootprintEnty::kFootprintEntrySize> FileFootprint;
typedef tsl::robin_map<ustring, FileFootprint> FileFootprintMap;

struct ImageCacheFootprint {
    static const ustring utotal;
    static const ustring uconstant;

    // basic infos
    size_t ic_mem     = 0;                      // image cache
    size_t ic_str_mem = 0, ic_str_count = 0;    // std::string
    size_t ic_tile_mem = 0, ic_tile_count = 0;  // tile
    size_t ic_thdi_mem = 0, ic_thdi_count = 0;  // thread info
    size_t ic_fgpt_mem = 0, ic_fgpt_count = 0;  // fingerprint

    FileFootprintMap fmap;
    template<FileFootprintEnty entry>
    void add(const size_t size, const ustring& format)
    {
        addInternal<entry>(size, utotal);
        addInternal<entry>(size, format);
    }

    template<FileFootprintEnty entry>
    void addInternal(const size_t size, const ustring& key)
    {
        std::pair<FileFootprintMap::iterator, bool> insert = fmap.insert(
            { key, FileFootprint() });
        FileFootprint& array = insert.first.value();
        if (insert.second)
            std::fill(array.begin(), array.end(),
                      0ul);  // std::array has no default init to zero

        // update memory entry
        array[entry] += size;

        // update memory entry counter if exists
        if constexpr (entry % 2 == 0 && entry <= kLevelInfoMem)
            array[entry + 1] += 1;
    }
};

const ustring ImageCacheFootprint::utotal    = ustring("total");
const ustring ImageCacheFootprint::uconstant = ustring("constant");

/// Fills the parameter with a memory breakdown of the ImageCache.
inline size_t
footprint(const ImageCacheImpl& ic, ImageCacheFootprint& output)
{
    // strings
    output.ic_str_count = ic.m_searchdirs.size() + 2;
    output.ic_str_mem   = heapsize(ic.m_searchdirs) + heapsize(ic.m_searchpath)
                        + heapsize(ic.m_plugin_searchpath);

    // thread info
    output.ic_thdi_count = ic.m_all_perthread_info.size();
    output.ic_thdi_mem   = heapsize(ic.m_all_perthread_info);

    // tile cache
    output.ic_tile_count = ic.m_tilecache.size();
    for (TileCache::iterator t = ic.m_tilecache.begin(),
                             e = ic.m_tilecache.end();
         t != e; ++t)
        output.ic_tile_mem += footprint(t->first) + footprint(t->second);

    // finger prints; we only account for references, this map does not own the files.
    constexpr size_t sizeofFingerprintPair = sizeof(ustring)
                                             + sizeof(ImageCacheFileRef);
    output.ic_fgpt_count = ic.m_fingerprints.size();
    output.ic_fgpt_mem   = output.ic_fgpt_count * sizeofFingerprintPair;

    // files; count the footprint of files, subimages, level infos, image inputs, image specs
    for (FilenameMap::iterator t = ic.m_files.begin(), e = ic.m_files.end();
         t != e; ++t) {
        // get file format ustring; files with empty file format are simply constant valued.
        const ImageCacheFile& file(*t->second);
        const ustring& format = !file.fileformat().empty()
                                    ? file.fileformat()
                                    : ImageCacheFootprint::uconstant;

        const size_t fileftp = footprint(t->first) + footprint(t->second);
        output.add<kMem>(fileftp, format);

        const size_t specftp = footprint(file.m_configspec);
        output.add<kSpecMem>(specftp, format);

        const size_t inputftp = footprint(file.m_input);
        output.add<kInputMem>(inputftp, format);

        // subimages
        for (int s = 0, send = file.subimages(); s < send; ++s) {
            const ImageCacheFile::SubimageInfo& sub(file.subimageinfo(s));
            const size_t subftp = footprint(sub);
            output.add<kSubImageMem>(subftp, format);

            // level infos
            for (const auto& level : sub.levels) {
                const size_t lvlftp = footprint(level);
                output.add<kLevelInfoMem>(lvlftp, format);

                // extra infos; there are two ImageSpec structures stored in each LevelInfos,
                // and they turn out to be memory heavy, so we further break that down next.
                const size_t lvlspecftp = footprint(level.m_spec)
                                          + footprint(level.nativespec);
                const size_t lvlattrftp
                    = (level.m_spec ? footprint(level.m_spec->extra_attribs)
                                    : 0)
                      + footprint(level.nativespec.extra_attribs);
                const size_t lvlchanftp
                    = (level.m_spec ? footprint(level.m_spec->channelnames) : 0)
                      + footprint(level.nativespec.channelnames);
                output.add<kLevelInfoSpecMem>(lvlspecftp, format);
                output.add<kLevelInfoSpecMembMem>(2 * sizeof(ImageSpec),
                                                  format);
                output.add<kLevelInfoSpecParmsMem>(lvlattrftp, format);
                output.add<kLevelInfoSpecChanMem>(lvlchanftp, format);
            }
        }
    }

    // update total memory
    output.ic_mem += output.ic_str_mem;
    output.ic_mem += output.ic_tile_mem;
    output.ic_mem += output.ic_thdi_mem;
    output.ic_mem += output.fmap.find(ImageCacheFootprint::utotal)->second[kMem];
    output.ic_mem += output.ic_fgpt_mem;

    return output.ic_mem;
}

inline void
printImageCacheMemory(std::ostream& out, const ImageCacheImpl& ic)
{
    // get memory data
    pvt::ImageCacheFootprint data;
    pvt::footprint(ic, data);

    // print image cache memory usage
    print(out, "  Cache : {}\n", Strutil::memformat(data.ic_mem));
    print(out, "    Strings : {}, count : {}\n",
          Strutil::memformat(data.ic_str_mem), data.ic_str_count);
    print(out, "    Thread info : {}, count : {}\n",
          Strutil::memformat(data.ic_thdi_mem), data.ic_thdi_count);
    print(out, "    Fingerprints : {}, count : {}\n",
          Strutil::memformat(data.ic_fgpt_mem), data.ic_fgpt_count);
    print(out, "    Tiles : {}, count : {}\n",
          Strutil::memformat(data.ic_tile_mem), data.ic_tile_count);
    print(out, "    Files : {}, count : {}\n",
          Strutil::memformat(data.fmap[ImageCacheFootprint::utotal][kMem]),
          data.fmap[ImageCacheFootprint::utotal][kCount]);

    // print file formats memory usage
    for (pvt::FileFootprintMap::const_iterator t = data.fmap.begin(),
                                               e = data.fmap.end();
         t != e; ++t) {
        if (t.key() == ImageCacheFootprint::utotal)
            continue;
        print(out, "      Format '{}' : {}, count : {}\n", t->first,
              Strutil::memformat(t.value()[kMem]), t.value()[kCount]);
        if (t.value()[kInputMem] > 0ul)
            print(out, "        Image inputs : {}, count : {}\n",
                  Strutil::memformat(t.value()[kInputMem]),
                  t.value()[kInputCount]);
        if (t.value()[kSpecMem] > 0ul)
            print(out, "        Image specs : {}, count : {}\n",
                  Strutil::memformat(t.value()[kSpecMem]),
                  t.value()[kSpecCount]);
        if (t.value()[kSubImageMem] > 0ul)
            print(out, "        Subimages : {}, count : {}\n",
                  Strutil::memformat(t.value()[kSubImageMem]),
                  t.value()[kSubImageCount]);
        if (t.value()[kLevelInfoMem] > 0ul)
            print(out, "          Level infos : {}, count : {}\n",
                  Strutil::memformat(t.value()[kLevelInfoMem]),
                  t.value()[kLevelInfoCount]);
        if (t.value()[kLevelInfoSpecMem] > 0ul)
            print(out, "            Image specs : {}, count : {}\n",
                  Strutil::memformat(t.value()[kLevelInfoSpecMem]),
                  t.value()[kLevelInfoCount] * 2);
        if (t.value()[kLevelInfoSpecMembMem] > 0ul)
            print(out, "              Members : {}\n",
                  Strutil::memformat(t.value()[kLevelInfoSpecMembMem]));
        if (t.value()[kLevelInfoSpecParmsMem] > 0ul)
            print(out, "              Extra attributes : {}\n",
                  Strutil::memformat(t.value()[kLevelInfoSpecParmsMem]));
        if (t.value()[kLevelInfoSpecChanMem] > 0ul)
            print(out, "              Channel names : {}\n",
                  Strutil::memformat(t.value()[kLevelInfoSpecChanMem]));
    }
}

}  // namespace pvt

OIIO_NAMESPACE_END