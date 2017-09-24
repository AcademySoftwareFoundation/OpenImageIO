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
#include <set>
#include <vector>
#include <memory>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/tiffutils.h>


#define DEBUG_EXIF_READ  0
#define DEBUG_EXIF_WRITE 0
#define DEBUG_EXIF_UNHANDLED 0



OIIO_NAMESPACE_BEGIN
namespace pvt {



inline const void *
dataptr (const TIFFDirEntry &td, array_view<const uint8_t> data,
         int offset_adjustment)
{
    int len = tiff_data_size (td);
    if (len <= 4)
        return (const char *)&td.tdir_offset;
    else {
        int offset = td.tdir_offset + offset_adjustment;
        if (offset < 0 || offset+len > (int)data.size())
            return nullptr;   // out of bounds!
        return (const char*)data.data() + offset;
    }
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



class OIIO_API TagMap {
public:
    TagMap (string_view mapname, array_view<const TagInfo> tag_table);
    ~TagMap ();

    /// Find a TagInfo record for the tag index. or nullptr if not found.
    const TagInfo * find (int tag) const;

    /// Find a TagInfo record for the named tag. or nullptr if not found.
    const TagInfo * find (string_view name) const;

    /// Return the name for the tag index.
    const char * name (int tag) const;

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

array_view<const ExplanationTableEntry> canon_explanation_table ();


void append_tiff_dir_entry (std::vector<TIFFDirEntry> &dirs,
                            std::vector<char> &data,
                            int tag, TIFFDataType type, size_t count,
                            const void *mydata, size_t offset_correction,
                            size_t offset_override=0);

void decode_ifd (const unsigned char *ifd, array_view<const uint8_t> buf,
                 ImageSpec &spec, const TagMap& tag_map,
                 std::set<size_t>& ifd_offsets_seen, bool swab=false,
                 int offset_adjustment=0);

void encode_canon_makernote (std::vector<char>& exifblob,
                             std::vector<TIFFDirEntry> &exifdirs,
                             const ImageSpec& spec, size_t offset_correction);

}  // end namespace pvt

OIIO_NAMESPACE_END

