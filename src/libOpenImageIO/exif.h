/*
  Copyright 2008 Larry Gritz et al. All Rights Reserved.

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

#include <cstdint>
#include <boost/container/flat_map.hpp>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/paramlist.h>


extern "C" {
#include "tiff.h"
}

// Some EXIF tags that don't seem to be in tiff.h
#ifndef EXIFTAG_SECURITYCLASSIFICATION
#define EXIFTAG_SECURITYCLASSIFICATION 37394
#endif
#ifndef EXIFTAG_IMAGEHISTORY
#define EXIFTAG_IMAGEHISTORY 37395
#endif

#ifdef TIFF_VERSION_BIG
// In old versions of TIFF, this was defined in tiff.h.  It's gone from
// "BIG TIFF" (libtiff 4.x), so we just define it here.

struct TIFFHeader {
    uint16_t tiff_magic;  /* magic number (defines byte order) */
    uint16_t tiff_version;/* TIFF version number */
    uint32_t tiff_diroff; /* byte offset to first directory */
};

struct TIFFDirEntry {
    uint16_t tdir_tag;    /* tag ID */
    uint16_t tdir_type;   /* data type -- see TIFFDataType enum */
    uint32_t tdir_count;  /* number of items; length in spec */
    uint32_t tdir_offset; /* byte offset to field data */
};
#endif



#define DEBUG_EXIF_READ  0
#define DEBUG_EXIF_WRITE 0
#define DEBUG_EXIF_UNHANDLED 0



OIIO_NAMESPACE_BEGIN
namespace pvt {


TypeDesc tiff_datatype_to_typedesc (int tifftype, int tiffcount=1);

inline TypeDesc tiff_datatype_to_typedesc (const TIFFDirEntry& dir) {
    return tiff_datatype_to_typedesc (dir.tdir_type, dir.tdir_count);
}


int tiff_data_size (const TIFFDirEntry &dir);

inline const void *
dataptr (const TIFFDirEntry &td, string_view data)
{
    int len = tiff_data_size (td);
    return (len <= 4) ? (const char *)&td.tdir_offset
                      : (data.data() + td.tdir_offset);
}



struct LabelIndex {
    int value;
    const char *label;
};


typedef std::string (*ExplainerFunc) (const ParamValue &p, const void *extradata);

struct ExplanationTableEntry {
    const char    *oiioname;
    ExplainerFunc  explainer;
    const void    *extradata;
};


std::string explain_justprint (const ParamValue &p, const void *extradata);
std::string explain_labeltable (const ParamValue &p, const void *extradata);



struct TagInfo {
    typedef void (*HandlerFunc)(const TagInfo& taginfo, const TIFFDirEntry& dir,
                                string_view buf, ImageSpec& spec);

    TagInfo (int tag, const char *name, TIFFDataType type,
             int count, HandlerFunc handler = nullptr)
        : tifftag(tag), name(name), tifftype(type), tiffcount(count),
          handler(handler) {}

    int tifftag = -1;                     // TIFF tag used for this info
    const char *name = nullptr;           // Attribute name we use
    TIFFDataType tifftype = TIFF_NOTYPE;  // Data type that TIFF wants
    int tiffcount = 0;                    // Number of items
    HandlerFunc handler = nullptr;        // Special handler
};



class TagMap {
    typedef boost::container::flat_map<int, const TagInfo *> tagmap_t;
    typedef boost::container::flat_map<std::string, const TagInfo *> namemap_t;
    // Name map is lower case so it's effectively case-insensitive
public:
    TagMap (string_view mapname, array_view<const TagInfo> tag_table)
        : m_mapname(mapname)
    {
        for (const auto& tag : tag_table) {
            m_tagmap[tag.tifftag] = &tag;
            if (tag.name) {
                std::string lowername (tag.name);
                Strutil::to_lower (lowername);
                m_namemap[lowername] = &tag;
            }
        }
    }

    const TagInfo * find (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? NULL : i->second;
    }

    const TagInfo * find (string_view name) const {
        std::string lowername (name);
        Strutil::to_lower (lowername);
        namemap_t::const_iterator i = m_namemap.find (lowername);
        return i == m_namemap.end() ? NULL : i->second;
    }

    const char * name (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? NULL : i->second->name;
    }

    TIFFDataType tifftype (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? TIFF_NOTYPE : i->second->tifftype;
    }

    int tiffcount (int tag) const {
        tagmap_t::const_iterator i = m_tagmap.find (tag);
        return i == m_tagmap.end() ? 0 : i->second->tiffcount;
    }

    int tag (string_view name) const {
        std::string lowername (name);
        Strutil::to_lower (lowername);
        namemap_t::const_iterator i = m_namemap.find (lowername);
        return i == m_namemap.end() ? -1 : i->second->tifftag;
    }

    string_view mapname() const { return m_mapname; }

private:
    tagmap_t m_tagmap;
    namemap_t m_namemap;
    std::string m_mapname;
};



array_view<const ExplanationTableEntry> canon_explanation_table ();
TagMap& canon_maker_tagmap_ref ();


void append_tiff_dir_entry (std::vector<TIFFDirEntry> &dirs,
                            std::vector<char> &data,
                            int tag, TIFFDataType type, size_t count,
                            const void *mydata, size_t offset_correction,
                            size_t offset_override=0);

void encode_canon_makernote (std::vector<char>& exifblob,
                             std::vector<TIFFDirEntry> &exifdirs,
                             const ImageSpec& spec, size_t offset_correction);

}  // end namespace pvt

OIIO_NAMESPACE_END

