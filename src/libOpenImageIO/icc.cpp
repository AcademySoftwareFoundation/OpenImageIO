// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <string>
#include <unordered_map>

#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/tiffutils.h>

OIIO_NAMESPACE_BEGIN

using namespace pvt;


namespace {

struct ICCDateTime {
    uint16_t year;
    uint16_t month;
    uint16_t day;
    uint16_t hours;
    uint16_t minutes;
    uint16_t seconds;

    void swap_endian()
    {
        if (littleendian()) {
            OIIO::swap_endian(&year);
            OIIO::swap_endian(&month);
            OIIO::swap_endian(&day);
            OIIO::swap_endian(&hours);
            OIIO::swap_endian(&minutes);
            OIIO::swap_endian(&seconds);
        }
    }
};

static_assert(sizeof(ICCDateTime) == 12, "ICCDateTime is not 12 bytes");



// In-memory representation of an ICC profile
struct ICCHeader {
    uint32_t profile_size;
    uint32_t cmm_type;
    uint8_t profile_version[4];  // major, (minor << 4 + patch), then unused
    char device_class[4];
    char color_space[4];
    char pcs[4];  // profile connection space
    ICCDateTime creation_date;
    uint32_t magic;  // should be 'acsp' or 0x61637370
    char platform_signature[4];
    uint32_t flags;
    uint32_t manufacturer;
    uint32_t model;
    uint32_t attributes[2];
    uint32_t rendering_intent;
    unsigned char illuminant[12];  // XYZNumber
    uint32_t creator_signature;
    unsigned char profile_id[16];
    unsigned char reserved[28];

    void swap_endian()
    {
        if (littleendian()) {
            OIIO::swap_endian(&profile_size);
            OIIO::swap_endian(&cmm_type);
            OIIO::swap_endian(&magic);
            creation_date.swap_endian();
            OIIO::swap_endian(&flags);
            OIIO::swap_endian(&manufacturer);
            OIIO::swap_endian(&model);
            OIIO::swap_endian(&attributes[0]);
            OIIO::swap_endian(&attributes[1]);
            OIIO::swap_endian(&rendering_intent);
            OIIO::swap_endian(&creator_signature);
        }
    }
};

static_assert(sizeof(ICCHeader) == 128, "ICCHeader is not 128 bytes");



struct ICCTag {
    char signature[4];
    uint32_t offset;
    uint32_t size;

    void swap_endian()
    {
        if (littleendian()) {
            OIIO::swap_endian(&offset);
            OIIO::swap_endian(&size);
        }
    }
};



static const char*
icc_device_class_name(const std::string& device_class)
{
    static const std::unordered_map<std::string, const char*> device_class_names
        = {
              { "scnr", "Input device profile" },
              { "mntr", "Display device profile" },
              { "prtr", "Output device profile" },
              { "link", "DeviceLink profile" },
              { "spac", "ColorSpace profile" },
              { "abst", "Abstract profile" },
              { "nmcl", "NamedColor profile" },
          };
    // std::unordered_map::operator[](const Key& key) will add the key to the map if
    // it doesn't exist. This isn't what is intended and isn't thread safe.
    // Instead, just do the lookup and return the value or a nullptr.
    //
    // return device_class_names[device_class];
    auto it = device_class_names.find(device_class);
    return (it != device_class_names.end()) ? it->second : nullptr;
}

static const char*
icc_color_space_name(string_view color_space)
{
    // clang-format off
    static const std::unordered_map<std::string, const char*> color_space_names = {
        { "XYZ ", "XYZ" },      { "Lab ", "CIELAB" },   { "Luv ", "CIELUV" },
        { "YCbr", "YCbCr" },    { "Yxy ", "CIEYxy" },   { "RGB ", "RGB" },
        { "GRAY", "Gray" },     { "HSV ", "HSV" },      { "HLS ", "HLS" },
        { "CMYK", "CMYK" },     { "CMY ", "CMY" },      { "2CLR", "2 color" },
        { "3CLR", "3 color" },  { "4CLR", "4 color" },  { "5CLR", "5 color" },
        { "6CLR", "6 color" },  { "7CLR", "7 color" },  { "8CLR", "8 color" },
        { "9CLR", "9 color" },  { "ACLR", "10 color" }, { "BCLR", "11 color" },
        { "BCLR", "12 color" }, { "CCLR", "13 color" }, { "DCLR", "14 color" },
        { "ECLR", "15 color" }, { "FCLR", "16 color" },
    };
    // clang-format on
    // std::unordered_map::operator[](const Key& key) will add the key to the map if
    // it doesn't exist. This isn't what is intended and isn't thread safe.
    // Instead, just do the lookup and return the value or a nullptr.
    //
    // return color_space_names[color_space];
    auto it = color_space_names.find(color_space);
    return (it != color_space_names.end()) ? it->second : nullptr;
}

static const char*
icc_primary_platform_name(const std::string& platform)
{
    static const std::unordered_map<std::string, const char*> primary_platforms
        = {
              { "APPL", "Apple Computer, Inc." },
              { "MSFT", "Microsoft Corporation" },
              { "SGI ", "Silicon Graphics, Inc." },
              { "SUNW", "Sun Microsystems, Inc." },
          };
    // std::unordered_map::operator[](const Key& key) will add the key to the map if
    // it doesn't exist. This isn't what is intended and isn't thread safe.
    // Instead, just do the lookup and return the value or a nullptr.
    //
    // return primary_platforms[platform];
    auto it = primary_platforms.find(platform);
    return (it != primary_platforms.end()) ? it->second : nullptr;
}

static const char*
icc_rendering_intent_name(uint32_t intent)
{
    static const char* rendering_intents[]
        = { "Perceptual", "Media-relative colorimetric", "Saturation",
            "ICC-absolute colorimetric" };
    return intent < 4 ? rendering_intents[intent] : "Unknown";
}

static std::string
icc_tag_name(const std::string& tag)
{
    static const std::unordered_map<std::string, std::string> tagnames = {
        { "targ", "characterization_target" },
        { "cprt", "copyright" },
        { "desc", "profile_description" },
        { "dmdd", "device_model_description" },
        { "dmnd", "device_manufacturer_description" },
        { "vued", "viewing_conditions_description" },
    };
    // std::unordered_map::operator[](const Key& key) will add the key to the map if
    // it doesn't exist. This isn't what is intended and isn't thread safe.
    // Instead, just do the lookup and return the value or an empty string.
    //
    //return tagnames[tag];
    auto it = tagnames.find(tag);
    return (it != tagnames.end()) ? it->second : std::string();
}



template<typename T>
bool
extract(cspan<uint8_t> iccdata, size_t& offset, T& result, std::string& error)
{
    if (offset + sizeof(T) > std::size(iccdata)) {
        error = "ICC profile too small";
        return false;
    }
    result = *(const T*)(iccdata.data() + offset);
    offset += sizeof(T);
    if (littleendian())
        swap_endian(&result);
    return true;
}

template<>
bool
extract(cspan<uint8_t> iccdata, size_t& offset, ICCTag& result,
        std::string& error)
{
    if (offset + sizeof(result) > std::size(iccdata)) {
        error = "ICC profile too small";
        return false;
    }
    result = *(const ICCTag*)(iccdata.data() + offset);
    offset += sizeof(ICCTag);
    if (littleendian())
        result.swap_endian();
    return true;
}

}  // namespace



bool
decode_icc_profile(cspan<uint8_t> iccdata, ImageSpec& spec, std::string& error)
{
    using Strutil::fmt::format;
    if (std::size(iccdata) < sizeof(ICCHeader)) {
        error = "ICC profile too small";
        return false;
    }
    ICCHeader header = *(const ICCHeader*)iccdata.data();
    header.swap_endian();
    if (header.magic != 0x61637370) {
        error = "ICC profile has bad magic number";
        return false;
    }
    if (header.profile_size != iccdata.size()) {
        error = "ICC profile size mismatch";
        return false;
    }

    spec.attribute("ICCProfile:profile_size", header.profile_size);
    spec.attribute("ICCProfile:cmm_type", header.cmm_type);
    spec.attribute("ICCProfile:profile_version",
                   format("{:d}.{:d}.{}", header.profile_version[0],
                          header.profile_version[1] >> 4,
                          header.profile_version[1] & 0xf));
    spec.attribute("ICCProfile:device_class",
                   icc_device_class_name(string_view(header.device_class, 4)));
    spec.attribute("ICCProfile:color_space",
                   icc_color_space_name(string_view(header.color_space, 4)));
    spec.attribute("ICCProfile:profile_connection_space",
                   icc_color_space_name(string_view(header.pcs, 4)));
    spec.attribute("ICCProfile:platform_signature",
                   icc_primary_platform_name(
                       string_view(header.platform_signature, 4)));
    spec.attribute("ICCProfile:creation_date",
                   format("{:02d}:{:02d}:{:02d} {:02d}:{:02d}:{:02d}",
                          header.creation_date.year, header.creation_date.month,
                          header.creation_date.day, header.creation_date.hours,
                          header.creation_date.minutes,
                          header.creation_date.seconds));
    spec.attribute("ICCProfile:flags",
                   format("{}, {}",
                          header.flags & 1 ? "Embedded " : "Not Embedded",
                          header.flags & 2 ? "Dependent" : "Independent"));
    spec.attribute("ICCProfile:manufacturer",
                   format("{:x}", header.manufacturer));
    spec.attribute("ICCProfile:model", format("{:x}", header.model));
    spec.attribute("ICCProfile:attributes",
                   format("{}, {}, {}, {}",
                          header.attributes[1] & 1 ? "Transparency "
                                                   : "Reflective",
                          header.attributes[1] & 2 ? "Matte" : "Glossy",
                          header.attributes[1] & 4 ? "Negative" : "Positive",
                          header.attributes[1] & 8 ? "Black & White" : "Color"));
    spec.attribute("ICCProfile:rendering_intent",
                   icc_rendering_intent_name(header.rendering_intent));
    // spec.attribute("ICCProfile:illuminant", header.illuminant);
    spec.attribute("ICCProfile:creator_signature",
                   format("{:x}", header.creator_signature));


    size_t offset = 128;
    uint32_t tag_count;
    if (!extract(iccdata, offset, tag_count, error))
        return false;
    for (uint32_t i = 0; i < tag_count; ++i) {
        ICCTag tag;
        if (!extract(iccdata, offset, tag, error))
            return false;
        string_view signature(tag.signature, 4);
        if (!check_span(iccdata, iccdata.data() + tag.offset,
                        std::max(4U, tag.size))) {
            error = format(
                "ICC profile tag {} appears to contain corrupted/invalid data",
                signature);
            return false;  // Non-sensical: tag extends beyond icc data block
        }
        string_view typesignature((const char*)iccdata.data() + tag.offset, 4);
        // Strutil::print("   tag {} type {} offset {} size {}\n", signature,
        //                typesignature, tag.offset, tag.size);
        std::string tagname = icc_tag_name(signature);
        if (tagname.empty())
            tagname = signature;
        tagname = format("ICCProfile:{}", tagname);
        if (typesignature == "text") {
            // For text, the first 4 bytes are "text", the next 4 are 0, then
            // byte 8-end are the zero-terminated string itself.
            if (tag.size < 8) {
                error = format(
                    "ICC profile tag {} appears to contain corrupted/invalid data",
                    signature);
                return false;
            }
            spec.attribute(tagname, string_view((const char*)iccdata.data()
                                                    + tag.offset + 8,
                                                tag.size - 8));
        } else if (typesignature == "desc") {
            // I don't see this in the spec, but I've seen it in practice:
            // first 4 bytes are "desc", next 8 are unknown, then 12-end are
            // zero-terminated string itself.
            if (tag.size < 12) {
                error = format(
                    "ICC profile tag {} appears to contain corrupted/invalid data",
                    signature);
                return false;
            }
            spec.attribute(tagname, string_view((const char*)iccdata.data()
                                                    + tag.offset + 12,
                                                tag.size - 12));
        } else if (typesignature == "mluc") {
            // Multi-localized unicode text.  First 4 bytes are "mluc", next 4
            // are 0, next 4 are the number of records, then 12-end are the
            // 12-byte records which each consist of a 2-byte language code,
            // 2-byte country code, 4-byte length of the string (in bytes) and
            // 4-byte offset (from the tag start, not from the ICC start!) to
            // a zero-terminated big endian utf-16 string. We're just going to
            // grab the english language version for now.
            size_t where = tag.offset + 4;  // we already read the "mluc"
            where += 4;                     // skip zero bytes
            uint32_t nrecords   = 0;
            uint32_t recordsize = 0;
            if (!extract(iccdata, where, nrecords, error)
                || !extract(iccdata, where, recordsize, error)
                || recordsize != 12) {
                return false;
            }
            for (uint32_t r = 0; r < nrecords; ++r) {
                uint16_t language = 0, country = 0;
                uint32_t len = 0, stroffset = 0;
                if (!extract(iccdata, where, language, error)
                    || !extract(iccdata, where, country, error)
                    || !extract(iccdata, where, len, error)
                    || !extract(iccdata, where, stroffset, error)) {
                    return false;
                }
                if (language == ('e' << 8) + 'n') {
                    // English
                    // Strutil::print(
                    //     "eng len={} stfoffset={} ({:x}) wcharsize={}\n", len,
                    //     stroffset, tag.offset + stroffset, sizeof(wchar_t));
                    if (!check_span(iccdata,
                                    iccdata.data() + tag.offset + stroffset,
                                    len)) {
                        error = format(
                            "ICC profile tag {} appears to contain corrupted/invalid data",
                            signature);
                        return false;  // Non-sensical: tag extends beyond icc data block
                    }
                    const char* start = (const char*)iccdata.data() + tag.offset
                                        + stroffset;
                    // The actual data is UTF-16
                    std::u16string wstr((const char16_t*)start, len / 2);
                    if (littleendian())
                        swap_endian((uint16_t*)wstr.data(), int(wstr.size()));
                    spec.attribute(tagname, Strutil::utf16_to_utf8(wstr));
                    break;
                }
            }
        }
    }
    return true;
}

OIIO_NAMESPACE_END
