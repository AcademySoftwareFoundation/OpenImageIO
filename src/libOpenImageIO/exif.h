// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/tiffutils.h>


#define DEBUG_EXIF_READ 0
#define DEBUG_EXIF_WRITE 0
#define DEBUG_EXIF_UNHANDLED 0


// Exif Documentation:
//   - https://archive.org/details/exif-specs-3.0-dc-008-translation-2023-e


OIIO_NAMESPACE_BEGIN
namespace pvt {



// Byte length of a directory entry's data, or 0 if we can't know it.
// tiff_data_size() reports an unrecognized type as size_t(-1), which must
// never reach the bounds arithmetic below: multiplied by the count it wraps,
// and a wrapped length passes any `offset + len > size` test.
inline size_t
dirdata_size(const TIFFDirEntry& td)
{
    size_t elemsize = tiff_data_size(TIFFDataType(td.tdir_type));
    if (elemsize == 0 || elemsize == size_t(-1))
        return 0;
    return elemsize * size_t(td.tdir_count);
}



inline const void*
dataptr(const TIFFDirEntry& td, cspan<uint8_t> data, int offset_adjustment)
{
    size_t len = dirdata_size(td);
    if (len == 0)
        return nullptr;  // unknown type or no data
    if (len <= 4)
        return (const char*)&td.tdir_offset;
    else {
        int64_t offset = int64_t(td.tdir_offset) + offset_adjustment;
        if (offset < 0 || uint64_t(offset) + len > uint64_t(std::size(data)))
            return nullptr;  // out of bounds!
        return (const char*)data.data() + offset;
    }
}



// Return a span that bounds offset data within the dir entry.
// No matter what the type T, we still return a cspan<uint8_t> because it's
// unaligned data somewhere in the middle of a byte array. We would have
// alignment errors if we try to access it as some other type if it's not
// properly aligned.
template<typename T>
inline cspan<uint8_t>
dataspan(const TIFFDirEntry& td, cspan<uint8_t> data, int offset_adjustment,
         size_t count)
{
    size_t len = dirdata_size(td);
    OIIO_DASSERT(len == sizeof(T) * count);
    if (len == 0)
        return {};  // unknown type or no data
    if (len <= 4)
        return { (const uint8_t*)&td.tdir_offset, span_size_t(len) };
    else {
        int64_t offset = int64_t(td.tdir_offset) + offset_adjustment;
        if (offset < 0 || uint64_t(offset) + len > uint64_t(std::size(data)))
            return {};  // out of bounds! return empty span
        return { data.data() + offset, span_size_t(len) };
    }
}



struct LabelIndex {
    int value;
    const char* label;
};


typedef std::string (*ExplainerFunc)(const ParamValue& p,
                                     const void* extradata);

struct ExplanationTableEntry {
    const char* oiioname;
    ExplainerFunc explainer;
    const void* extradata;
};


std::string explain_justprint (const ParamValue &p, const void *extradata);
std::string explain_labeltable (const ParamValue &p, const void *extradata);



class OIIO_API TagMap {
public:
    TagMap (string_view mapname, cspan<TagInfo> tag_table);
    ~TagMap ();

    /// Find a TagInfo record for the tag index. or nullptr if not found.
    const TagInfo* find(int tag) const;

    /// Find a TagInfo record for the named tag. or nullptr if not found.
    const TagInfo* find(string_view name) const;

    /// Return the name for the tag index.
    const char* name(int tag) const;

    /// Return a TIFFDataType, given a tag index.
    TIFFDataType tifftype (int tag) const;

    /// Return a data item count, given a tag index.
    int tiffcount (int tag) const;

    /// Return the tag number, given a tag name.
    int tag (string_view name) const;

    /// Return the name of the map
    string_view mapname() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};



const TagMap& tiff_tagmap_ref ();
const TagMap& exif_tagmap_ref ();
const TagMap& gps_tagmap_ref ();
const TagMap& canon_maker_tagmap_ref ();

cspan<ExplanationTableEntry> canon_explanation_table ();


void append_tiff_dir_entry (std::vector<TIFFDirEntry> &dirs,
                            std::vector<char> &data,
                            int tag, TIFFDataType type, size_t count,
                            cspan<std::byte> mydata, size_t offset_correction,
                            size_t offset_override = 0,
                            OIIO::endian endianreq = OIIO::endian::native);

// Decode one IFD and everything it points at. `depth` is the IFD nesting
// level, incremented on each recursive step and used to bound the recursion.
bool decode_ifd (cspan<uint8_t> buf, size_t ifd_offset,
                 ImageSpec &spec, const TagMap& tag_map,
                 std::set<size_t>& ifd_offsets_seen, bool swab=false,
                 int offset_adjustment=0, int depth=0);

void encode_canon_makernote (std::vector<char>& exifblob,
                             std::vector<TIFFDirEntry> &exifdirs,
                             const ImageSpec& spec, size_t offset_correction);

}  // end namespace pvt

OIIO_NAMESPACE_END

