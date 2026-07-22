// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <Ptexture.h>

#include <cstddef>
#include <cstring>

#include <OpenImageIO/bit.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/span.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


class PtexInput final : public ImageInput {
public:
    PtexInput()
        : m_ptex(NULL)
    {
        init();
    }
    ~PtexInput() override { close(); }
    const char* format_name(void) const override { return "ptex"; }
    int supports(string_view feature) const override
    {
        return (feature == "arbitrary_metadata"
                || feature == "exif"  // Because of arbitrary_metadata
                || feature == "iptc"  // Because of arbitrary_metadata
                || feature == "multiimage" || feature == "mipmap");
    }
    bool valid_file(const std::string& filename) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool close() override;
    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    int current_miplevel(void) const override
    {
        lock_guard lock(*this);
        return m_miplevel;
    }
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;

private:
    PtexTexture* m_ptex;
    int m_subimage;
    int m_miplevel;
    int m_numFaces;
    Ptex::Res m_faceres;
    Ptex::Res m_mipfaceres;
    Ptex::Res m_tileres;
    bool m_isTiled;
    bool m_hasMipMaps;
    int m_ntilesu;

    /// Reset everything to initial state
    ///
    void init()
    {
        if (m_ptex)
            m_ptex->release();
        m_ptex     = NULL;
        m_subimage = -1;
        m_miplevel = -1;
    }

    void get_ptex_metadata(PtexMetaData* pmeta);
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
ptex_input_imageio_create()
{
    return new PtexInput;
}

OIIO_EXPORT int ptex_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
ptex_imageio_library_version()
{
    return ustring::fmtformat("Ptex {}.{}", PtexLibraryMajorVersion,
                              PtexLibraryMinorVersion)
        .c_str();
}

OIIO_EXPORT const char* ptex_input_extensions[] = { "ptex", "ptx", nullptr };

OIIO_PLUGIN_EXPORTS_END



namespace {

// The on-disk Ptex file header is exactly 64 bytes, stored little-endian. See
// the format documentation at https://ptex.us/PtexFile.html and the Header
// struct in PtexIO.h of the Ptex distribution. We parse and sanity-check it
// ourselves before handing the file to PtexTexture::open(), which does a poor
// job of detecting corrupt headers and is prone to over-allocating based on
// bogus face/level/channel counts.
constexpr int ptex_header_size = 64;

// Generous sanity caps -- far larger than any real Ptex file, but small enough
// to reject wildly corrupt headers before they drive huge allocations. nlevels
// is a per-face mip count, so the cap of 20 already implies a maximum face
// resolution of 2^20 in each dimension -- well beyond anything real.
constexpr uint32_t ptex_max_channels = 1024;
constexpr uint32_t ptex_max_levels   = 20;

// Maximum plausible expansion of a zlib-compressed block. zlib's theoretical
// worst case is ~1032:1; we use a looser bound so that we never reject a valid
// file, only headers whose claimed uncompressed size could not possibly have
// come from the (smaller) number of compressed bytes actually in the file.
constexpr uint64_t ptex_max_zip_ratio = 4096;

// Sizes of the fixed on-disk records, used for consistency checks:
//   FaceInfo  = Res[2] + adjedges[1] + flags[1] + adjfaces[4*4]
//   LevelInfo = leveldatasize[8] + levelheadersize[4] + nfaces[4]
constexpr uint64_t ptex_faceinfo_size  = 20;
constexpr uint64_t ptex_levelinfo_size = 16;


// Parsed, host-endian copy of the 64-byte on-disk Ptex header. Constructed
// directly from the raw little-endian header bytes; each field is byte-swapped
// on big-endian hosts. See https://ptex.us/PtexFile.html and the Header struct
// in PtexIO.h of the Ptex distribution.
struct PtexHeader {
    char magic[4];
    uint32_t version;
    uint32_t meshtype;
    uint32_t datatype;
    int32_t alphachan;
    uint16_t nchannels;
    uint16_t nlevels;
    uint32_t nfaces;
    uint32_t extheadersize;
    uint32_t faceinfosize;
    uint32_t constdatasize;
    uint32_t levelinfosize;
    uint32_t minorversion;
    uint64_t leveldatasize;
    uint32_t metadatazipsize;
    uint32_t metadatamemsize;

    // Parse from at least `ptex_header_size` bytes of raw header data. The
    // struct has no padding (see the static_assert below), so the on-disk
    // layout can be copied in one shot; integer fields are then byte-swapped on
    // big-endian hosts (the magic is a byte sequence and needs no swap).
    explicit PtexHeader(cspan<std::byte> b)
    {
        OIIO_DASSERT(b.size() >= size_t(ptex_header_size));
        std::memcpy(this, b.data(), ptex_header_size);
        if (bigendian()) {
            version         = byteswap(version);
            meshtype        = byteswap(meshtype);
            datatype        = byteswap(datatype);
            alphachan       = byteswap(alphachan);
            nchannels       = byteswap(nchannels);
            nlevels         = byteswap(nlevels);
            nfaces          = byteswap(nfaces);
            extheadersize   = byteswap(extheadersize);
            faceinfosize    = byteswap(faceinfosize);
            constdatasize   = byteswap(constdatasize);
            levelinfosize   = byteswap(levelinfosize);
            minorversion    = byteswap(minorversion);
            leveldatasize   = byteswap(leveldatasize);
            metadatazipsize = byteswap(metadatazipsize);
            metadatamemsize = byteswap(metadatamemsize);
        }
    }

    bool valid_magic() const
    {
        return magic[0] == 'P' && magic[1] == 't' && magic[2] == 'e'
               && magic[3] == 'x';
    }
};

static_assert(sizeof(PtexHeader) == ptex_header_size,
              "PtexHeader must exactly match the 64-byte on-disk Ptex header");



// Validate the Ptex header at the start of a file held in `b`, given the total
// file size. On any failure, set `err` to a human-readable reason and return
// false. This catches the great majority of corrupt or malicious headers that
// PtexTexture::open() would either miss or respond to by over-allocating.
bool
ptex_validate_header(cspan<std::byte> b, uint64_t filesize, std::string& err)
{
    if (b.size() < size_t(ptex_header_size)) {
        err = "file is too small to contain a Ptex header";
        return false;
    }
    PtexHeader h(b);

    if (!h.valid_magic()) {
        err = "not a Ptex file (wrong magic number)";
        return false;
    }
    if (h.version != 1) {
        err = Strutil::format("unsupported Ptex file version {}", h.version);
        return false;
    }
    if (h.meshtype > 1) {  // mt_triangle, mt_quad
        err = Strutil::format("invalid Ptex mesh type {}", h.meshtype);
        return false;
    }
    if (h.datatype > 3) {  // dt_uint8, dt_uint16, dt_half, dt_float
        err = Strutil::format("invalid Ptex data type {}", h.datatype);
        return false;
    }
    if (h.nchannels < 1 || h.nchannels > ptex_max_channels) {
        err = Strutil::format("unreasonable Ptex channel count {}",
                              h.nchannels);
        return false;
    }
    if (h.alphachan != -1
        && (h.alphachan < 0 || uint32_t(h.alphachan) >= h.nchannels)) {
        err = Strutil::format("invalid Ptex alpha channel {}", h.alphachan);
        return false;
    }
    if (h.nfaces < 1) {
        err = "Ptex file has no faces";
        return false;
    }
    if (h.nlevels < 1 || h.nlevels > ptex_max_levels) {
        err = Strutil::format("unreasonable Ptex level count {}", h.nlevels);
        return false;
    }
    // The level-info block is stored uncompressed as exactly nlevels
    // fixed-size records, so its size must match precisely.
    if (h.levelinfosize != uint64_t(h.nlevels) * ptex_levelinfo_size) {
        err = "inconsistent Ptex level info size (corrupt header?)";
        return false;
    }
    // Every on-disk block must fit within the actual file. (Optional edit
    // blocks may follow, so the sum can be smaller than the file, but never
    // larger.) Check leveldatasize first so the running sum cannot overflow.
    if (h.leveldatasize > filesize) {
        err = "Ptex level data size exceeds the file size (corrupt header?)";
        return false;
    }
    uint64_t claimed = uint64_t(ptex_header_size) + h.extheadersize
                       + h.faceinfosize + h.constdatasize + h.levelinfosize
                       + h.metadatazipsize + h.leveldatasize;
    if (claimed > filesize) {
        err = "Ptex header block sizes exceed the file size (corrupt header?)";
        return false;
    }
    // The compressed blocks below are inflated into freshly-allocated buffers
    // whose sizes are taken from the header. Reject any header whose claimed
    // uncompressed size could not plausibly have been produced from the
    // on-disk compressed bytes -- this is the main defense against headers
    // engineered to provoke huge allocations.
    static const uint64_t dtsize[4] = { 1, 2, 2, 4 };
    const uint64_t pixelsize        = dtsize[h.datatype] * h.nchannels;
    // faceinfo: read into an array resized to nfaces (nfaces * FaceInfo bytes).
    if (uint64_t(h.nfaces) * ptex_faceinfo_size
        > uint64_t(h.faceinfosize) * ptex_max_zip_ratio) {
        err = Strutil::format("unreasonable Ptex face count {}", h.nfaces);
        return false;
    }
    // constdata: one constant pixel per face (nfaces * pixelsize bytes).
    if (uint64_t(h.nfaces) * pixelsize
        > uint64_t(h.constdatasize) * ptex_max_zip_ratio) {
        err = "unreasonable Ptex constant-data size (corrupt header?)";
        return false;
    }
    // metadata: inflated from metadatazipsize to metadatamemsize.
    if (uint64_t(h.metadatamemsize)
        > uint64_t(h.metadatazipsize) * ptex_max_zip_ratio) {
        err = "unreasonable Ptex metadata size (corrupt header?)";
        return false;
    }
    return true;
}

}  // namespace



bool
PtexInput::valid_file(const std::string& filename) const
{
    std::byte header[ptex_header_size];
    size_t n = Filesystem::read_bytes(filename, header, ptex_header_size);
    std::string err;
    return ptex_validate_header(cspan<std::byte>(header, n),
                                Filesystem::file_size(filename), err);
}



bool
PtexInput::open(const std::string& name, ImageSpec& newspec)
{
    // Validate the header ourselves before handing off to PtexTexture::open(),
    // which is poor at detecting corruption and prone to over-allocating on
    // bogus face/level/channel counts.
    {
        std::byte header[ptex_header_size];
        size_t n = Filesystem::read_bytes(name, header, ptex_header_size);
        std::string err;
        if (!ptex_validate_header(cspan<std::byte>(header, n),
                                  Filesystem::file_size(name), err)) {
            errorfmt("{}", err);
            return false;
        }
    }

    Ptex::String perr;
    m_ptex = PtexTexture::open(name.c_str(), perr, true /*premultiply*/);
    if (!perr.empty()) {
        if (m_ptex) {
            m_ptex->release();
            m_ptex = NULL;
        }
        errorfmt("{}", perr.c_str());
        return false;
    }

    m_numFaces   = m_ptex->numFaces();
    m_hasMipMaps = m_ptex->hasMipMaps();

    bool ok = seek_subimage(0, 0);
    newspec = spec();
    return ok;
}



bool
PtexInput::seek_subimage(int subimage, int miplevel)
{
    if (m_subimage == subimage && m_miplevel == miplevel)
        return true;  // Already fine

    if (subimage < 0 || subimage >= m_numFaces)
        return false;
    m_subimage                  = subimage;
    const Ptex::FaceInfo& pface = m_ptex->getFaceInfo(subimage);
    m_faceres                   = pface.res;

    int nmiplevels = std::max(m_faceres.ulog2, m_faceres.vlog2) + 1;
    if (miplevel < 0 || miplevel > nmiplevels - 1)
        return false;
    m_miplevel   = miplevel;
    m_mipfaceres = Ptex::Res(std::max(0, m_faceres.ulog2 - miplevel),
                             std::max(0, m_faceres.vlog2 - miplevel));

    TypeDesc format = TypeDesc::UNKNOWN;
    switch (m_ptex->dataType()) {
    case Ptex::dt_uint8: format = TypeDesc::UINT8; break;
    case Ptex::dt_uint16: format = TypeDesc::UINT16; break;
    case Ptex::dt_half: format = TypeDesc::HALF; break;
    case Ptex::dt_float: format = TypeDesc::FLOAT; break;
    default: errorfmt("Ptex with unknown data format"); return false;
    }

    m_spec = ImageSpec(std::max(1, m_faceres.u() >> miplevel),
                       std::max(1, m_faceres.v() >> miplevel),
                       m_ptex->numChannels(), format);

    m_spec.alpha_channel = m_ptex->alphaChannel();

    if (m_ptex->meshType() == Ptex::mt_triangle)
        m_spec.attribute("ptex:meshType", "triangle");
    else
        m_spec.attribute("ptex:meshType", "quad");

    if (m_ptex->hasEdits())
        m_spec.attribute("ptex:hasEdits", (int)1);

    PtexFaceData* facedata = m_ptex->getData(m_subimage, m_faceres);
    m_isTiled              = facedata->isTiled();
    if (m_isTiled) {
        m_tileres          = facedata->tileRes();
        m_spec.tile_width  = m_tileres.u();
        m_spec.tile_height = m_tileres.v();
        m_ntilesu          = m_faceres.ntilesu(m_tileres);
    } else {
        // Always make it look tiled
        m_spec.tile_width  = m_spec.width;
        m_spec.tile_height = m_spec.height;
    }

    std::string wrapmode;
    if (m_ptex->uBorderMode() == Ptex::m_clamp)
        wrapmode = "clamp";
    else if (m_ptex->uBorderMode() == Ptex::m_black)
        wrapmode = "black";
    else  // if (m_ptex->uBorderMode() == Ptex::m_periodic)
        wrapmode = "periodic";
    wrapmode += ",";
    if (m_ptex->uBorderMode() == Ptex::m_clamp)
        wrapmode += "clamp";
    else if (m_ptex->uBorderMode() == Ptex::m_black)
        wrapmode += "black";
    else  // if (m_ptex->uBorderMode() == Ptex::m_periodic)
        wrapmode += "periodic";
    m_spec.attribute("wrapmode", wrapmode);

    // Add the arbitrary metadata. For Ptex, we only add full metadata to the
    // first MIP level of the first subimage. The PTex format doesn't permit
    // metadata to differ per-face anyway.
    // Add the number of subimages as an attribute for the first spec.
    if (subimage == 0 && miplevel == 0) {
        if (PtexMetaData* pmeta = m_ptex->getMetaData()) {
            get_ptex_metadata(pmeta);
            pmeta->release();
        }

        m_spec.attribute("oiio:subimages", m_numFaces);
    }

    // Add the number of miplevels as an attribute for the first miplevel.
    if (miplevel == 0 && nmiplevels > 1)
        m_spec.attribute("oiio:miplevels", nmiplevels);

    facedata->release();
    return true;
}



void
PtexInput::get_ptex_metadata(PtexMetaData* pmeta)
{
    if (!pmeta)
        return;

        // Helper macro to get metadata of a specific type
#define GETMETA(pmeta, key, ptype, basetype, typedesc, value) \
    {                                                         \
        const ptype* v;                                       \
        int count;                                            \
        pmeta->getValue(key, v, count);                       \
        typedesc = TypeDesc(basetype, count);                 \
        value    = (const void*)v;                            \
    }

    int n = pmeta->numKeys();
    for (int i = 0; i < n; ++i) {
        const char* key = NULL;
        Ptex::MetaDataType ptype;
        pmeta->getKey(i, key, ptype);
        OIIO_DASSERT(key);
        const char* vchar;
        const void* value;
        TypeDesc typedesc;
        switch (ptype) {
        case Ptex::mdt_string:
            pmeta->getValue(key, vchar);
            value    = &vchar;
            typedesc = TypeDesc::STRING;
            break;
        case Ptex::mdt_int8:
            GETMETA(pmeta, key, int8_t, TypeDesc::INT8, typedesc, value);
            break;
        case Ptex::mdt_int16:
            GETMETA(pmeta, key, int16_t, TypeDesc::INT16, typedesc, value);
            break;
        case Ptex::mdt_int32:
            GETMETA(pmeta, key, int32_t, TypeDesc::INT32, typedesc, value);
            break;
        case Ptex::mdt_float:
            GETMETA(pmeta, key, float, TypeDesc::FLOAT, typedesc, value);
            break;
        case Ptex::mdt_double:
            GETMETA(pmeta, key, double, TypeDesc::DOUBLE, typedesc, value);
            break;
        default: continue;
        }
        m_spec.attribute(key, typedesc, value);
    }
}



bool
PtexInput::close()
{
    init();  // Reset to initial state, including closing any open files
    return true;
}



bool
PtexInput::read_native_scanline(int /*subimage*/, int /*miplevel*/, int /*y*/,
                                int /*z*/, void* /*data*/)
{
    return false;  // Not scanline oriented
}



bool
PtexInput::read_native_tile(int subimage, int miplevel, int x, int y, int /*z*/,
                            void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    PtexFaceData* facedata = m_ptex->getData(m_subimage, m_mipfaceres);

    PtexFaceData* f = facedata;
    if (m_isTiled) {
        int tileno = y / m_spec.tile_height * m_ntilesu + x / m_spec.tile_width;
        f          = facedata->getTile(tileno);
    }

    bool ok        = true;
    void* tiledata = f->getData();
    if (tiledata) {
        memcpy(data, tiledata, m_spec.tile_bytes());
    } else {
        ok = false;
    }

    if (m_isTiled)
        f->release();
    facedata->release();
    return ok;
}



OIIO_PLUGIN_NAMESPACE_END
