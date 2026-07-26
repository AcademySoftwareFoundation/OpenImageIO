// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <limits>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

#if OIIO_GNUC_VERSION >= 60000
#    pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif

#include <openvdb/openvdb.h>
#include <openvdb/tools/Dense.h>



OIIO_PLUGIN_NAMESPACE_BEGIN

struct layerrecord {
    std::string name;
    std::string attribute;
    openvdb::CoordBBox bounds;
    ImageSpec spec;
    openvdb::GridBase::Ptr grid;

    layerrecord(std::string obj, std::string attr, openvdb::CoordBBox bx,
                ImageSpec is, openvdb::GridBase::Ptr grd)
        : name(std::move(obj))
        , attribute(std::move(attr))
        , bounds(std::move(bx))
        , spec(std::move(is))
        , grid(std::move(grd))
    {
    }
};



class OpenVDBInput final : public ImageInput {
    std::string m_name;
    std::unique_ptr<openvdb::io::File> m_input;
    int m_subimage;    ///< What subimage/field are we looking at?
    int m_nsubimages;  ///< How many fields in the file?
    std::vector<layerrecord> m_layers;

    void init()
    {
        OIIO_DASSERT(!m_input);
        std::string().swap(m_name);
        std::vector<layerrecord>().swap(m_layers);
        m_subimage   = -1;
        m_nsubimages = 0;
    }

    void readMetaData(const openvdb::GridBase& grid, const layerrecord& layer,
                      ImageSpec& spec);

public:
    OpenVDBInput() { init(); }
    ~OpenVDBInput() override { close(); }

    const char* format_name(void) const override { return "openvdb"; }
    int supports(string_view feature) const override
    {
        return (feature == "arbitrary_metadata" || feature == "multiimage");
    }
    bool valid_file(const std::string& filename) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool close() override;
    int current_subimage(void) const override;
    bool seek_subimage(int subimage, int miplevel) override;
    bool seek_subimage_nolock(int subimage, int miplevel);
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;

    ImageSpec spec(int subimage, int miplevel) override;
    ImageSpec spec_dimensions(int subimage, int miplevel) override;
};



using namespace openvdb;



bool
OpenVDBInput::close()
{
    if (m_input) {
        m_input->close();
        m_input.reset();
    }

    init();  // Reset to initial state
    return true;
}



ImageSpec
OpenVDBInput::spec(int subimage, int miplevel)
{
    if (subimage < 0 || subimage >= m_nsubimages)  // out of range
        return ImageSpec();
    if (miplevel != 0)
        return ImageSpec();
    return m_layers[subimage].spec;
}



ImageSpec
OpenVDBInput::spec_dimensions(int subimage, int miplevel)
{
    if (subimage < 0 || subimage >= m_nsubimages)  // out of range
        return ImageSpec();
    if (miplevel != 0)
        return ImageSpec();
    ImageSpec spec;
    spec.copy_dimensions(m_layers[subimage].spec);
    return spec;
}



int
OpenVDBInput::current_subimage(void) const
{
    lock_guard lock(*this);
    return m_subimage;
}



bool
OpenVDBInput::seek_subimage(int subimage, int miplevel)
{
    lock_guard lock(*this);
    return seek_subimage_nolock(subimage, miplevel);
}



bool
OpenVDBInput::seek_subimage_nolock(int subimage, int miplevel)
{
    if (subimage < 0 || subimage >= m_nsubimages)  // out of range
        return false;
    if (miplevel != 0)
        return false;
    if (subimage == m_subimage)
        return true;

    m_subimage = subimage;
    m_spec     = m_layers[subimage].spec;
    return true;
}



namespace {

// OpenVDB builds exception text out of strings whose lengths came straight
// from the file, so a corrupt input can hand back a message of essentially
// arbitrary size. Clamp it before it reaches the error log.
inline string_view
clamped_what(const std::exception& e)
{
    return string_view(e.what()).substr(0, 1024);
}



CoordBBox
getBoundingBox(const GridBase& grid)
{
    auto bbMin = grid.getMetadata<TypedMetadata<Vec3i>>(
        GridBase::META_FILE_BBOX_MIN);
    if (bbMin) {
        auto bbMax = grid.getMetadata<TypedMetadata<Vec3i>>(
            GridBase::META_FILE_BBOX_MAX);
        if (bbMax)
            return CoordBBox(Coord(bbMin->value()), Coord(bbMax->value()));
    }
    return grid.evalActiveVoxelBoundingBox();
}



template<typename GridType> struct VDBReader {
    using TreeType  = typename GridType::TreeType;
    using RootType  = typename TreeType::RootNodeType;
    using Int1Type  = typename RootType::ChildNodeType;
    using Int2Type  = typename Int1Type::ChildNodeType;
    using ValueType = typename GridType::ValueType;
    using LeafType  = typename TreeType::LeafNodeType;
    typedef openvdb::tools::Dense<ValueType, openvdb::tools::LayoutXYZ> DenseT;

    // Both fill paths below write exactly LeafType::SIZE values into the
    // caller's tile buffer, which is sized from the tile dimensions that
    // fillSpec() publishes. Keep the two in agreement.
    static_assert(LeafType::SIZE
                      == LeafType::DIM * LeafType::DIM * LeafType::DIM,
                  "Leaf node size does not match the published tile size");

    static void setTile(ValueType* data, const ValueType value)
    {
        for (ValueType* end = data + LeafType::SIZE; data < end; ++data)
            *data = value;
    }

    static bool readTile(const GridType& grid, int x, int y, int z,
                         ValueType* values)
    {
        // Probe for a cell-centered voxel
        enum { kOffset = LeafType::DIM / 2 };
        // const int kOffset = LeafType::DIM / 2;
        const openvdb::Coord xyz(x + kOffset, y + kOffset, z + kOffset);
        const RootType& root = grid.tree().root();
        // Use the GridType::ConstAccessor so only one query needs to be done.
        // From that query, check the node type from 'most interesting' to least
        typename GridType::ConstAccessor cache = grid.getConstAccessor();
        if (auto* leaf = root.probeConstLeafAndCache(xyz, cache)) {
            CoordBBox bbox = leaf->getNodeBoundingBox();
            if (bbox.min().x() != x || bbox.min().y() != y
                || bbox.min().z() != z || bbox.dim() != Coord(LeafType::DIM))
                return false;  // unaligned or unexpected tile dimensions
            // Have OpenVDB fill the dense block, into the values pointer
            DenseT dense(bbox, values);
            leaf->copyToDense(bbox, dense);
        } else
            setTile(values, cache.getValue(xyz));
        return true;
    }

    // Fill in the spec's geometry from the grid bounds. The bounds may come
    // straight from untrusted file metadata, so every extent is computed in
    // 64 bits and range-checked before it is narrowed to the int fields of
    // ImageSpec. Return false if the bounds can't be represented; the caller
    // reports the error. check_open() applies the size policy afterwards.
    static bool fillSpec(const CoordBBox& bounds, ImageSpec& spec)
    {
        // Round the bounds outward to encompass the leaf-node dimension
        // (generally 8), so a box spanning [-2, -2, -2] -> [2, 2, 2]
        // is expanded to [-8, -8, -8] -> [8, 8, 8]. LeafType::DIM is a power
        // of two, so masking rounds down correctly for negative coordinates.
        static_assert((LeafType::DIM & (LeafType::DIM - 1)) == 0,
                      "Leaf node dimension is not a power of two");
        const int64_t mask = int64_t(LeafType::DIM) - 1;
        int64_t data_min[3], data_max[3], dim[3];
        for (int i = 0; i < 3; ++i) {
            const int64_t bmin = bounds.min()[i];
            const int64_t bmax = bounds.max()[i];
            data_min[i]        = bmin & ~mask;
            data_max[i] = bmax + (int64_t(LeafType::DIM) - (bmax & mask));
            dim[i]      = bmax - bmin + 1;
            if (dim[i] < 1 || dim[i] > std::numeric_limits<int>::max()
                || data_min[i] < std::numeric_limits<int>::min()
                || data_max[i] > std::numeric_limits<int>::max()
                || data_max[i] - data_min[i] + 1
                       > std::numeric_limits<int>::max())
                return false;
        }
        spec.x = int(data_min[0]);
        spec.y = int(data_min[1]);
        spec.z = int(data_min[2]);

        spec.width  = int(data_max[0] - data_min[0] + 1);
        spec.height = int(data_max[1] - data_min[1] + 1);
        spec.depth  = int(data_max[2] - data_min[2] + 1);

        spec.full_x = bounds.min().x();
        spec.full_y = bounds.min().y();
        spec.full_z = bounds.min().z();

        spec.full_width  = int(dim[0]);
        spec.full_height = int(dim[1]);
        spec.full_depth  = int(dim[2]);

        spec.tile_width  = LeafType::DIM;
        spec.tile_height = LeafType::DIM;
        spec.tile_depth  = LeafType::DIM;
        return true;
    }
};



// openvdb::io::File seems to not autoclose on destruct?
class VDBFile {
    std::unique_ptr<openvdb::io::File> m_file;

public:
    VDBFile(openvdb::io::File* f)
        : m_file(f)
    {
    }
    VDBFile(VDBFile&& rhs)
        : m_file(std::move(rhs.m_file))
    {
    }
    ~VDBFile()
    {
        if (m_file)
            m_file->close();
    }
    openvdb::io::File* operator->() { return m_file.get(); };
    operator bool() const { return m_file.get() != nullptr; }
};



VDBFile
openVDB(const std::string& filename, const ImageInput* errReport)
{
    if (!Filesystem::is_regular(filename))
        return nullptr;

    FILE* f = Filesystem::fopen(filename, "rb");
    if (!f)
        return nullptr;

    // Endianness of OPENVDB_MAGIC isn't clear, so just leave as is
    int32_t magic;
    static_assert(sizeof(magic) == sizeof(OPENVDB_MAGIC),
                  "Magic type not the same size");

    if (fread(&magic, sizeof(magic), 1, f) != 1)
        magic = 0;
    fclose(f);
    if (magic != OPENVDB_MAGIC)
        return nullptr;

    const char* errhint = "Unknown error";
    try {
        static struct OpenVDBLib {
            OpenVDBLib() { openvdb::initialize(); }
            ~OpenVDBLib() { openvdb::uninitialize(); }
        } sVDBLib;

        VDBFile file(new io::File(filename));

        file->open();
        if (file->isOpen())
            return file;

    } catch (const std::exception& e) {
        errReport->errorfmt("Could not open '{}': {}", filename,
                            clamped_what(e));
        return nullptr;
    } catch (...) {
        errhint = "Unknown exception thrown";
    }

    errReport->errorfmt("Could not open '{}': {}", filename, errhint);
    return nullptr;
}

}  // anonymous namespace



bool
OpenVDBInput::valid_file(const std::string& filename) const
{
    return openVDB(filename, this);
}



void
OpenVDBInput::readMetaData(const openvdb::GridBase& grid,
                           const layerrecord& layer, ImageSpec& spec)
{
    // If two grids of the same name exist in a VDB, then there will be an
    // object name & a grid name that get concatenated to make a unique name
    // "density[0].density", "density[1].density" for lookup.
    // Otherwise, just use the grid name; so one can do texture3d("Cd") instead
    // of texture3d("Cd.Cd")
    if (layer.name != layer.attribute)
        spec.attribute("oiio:subimagename", layer.name + "." + layer.attribute);
    else
        spec.attribute("oiio:subimagename", layer.attribute);

    auto mdPrefix = [](const openvdb::Name name) { return "openvdb:" + name; };

    const auto& transform   = grid.transform();
    const auto& map         = transform.baseMap()->getAffineMap();
    openvdb::math::Mat4d md = map->getConstMat4();

    static_assert(sizeof(openvdb::math::Mat4d) == sizeof(Imath::M44d),
                  "Matrix is not the right type / size!");

    spec.attribute(mdPrefix("indextoworld"),
                   TypeDesc(TypeDesc::DOUBLE, TypeDesc::MATRIX44), &md);

    // Invert to go from world to index
    md = md.inverse();

    spec.attribute(mdPrefix("worldtoindex"),
                   TypeDesc(TypeDesc::DOUBLE, TypeDesc::MATRIX44), &md);

    // Build the 'worldtolocal' matrix that OIIO wants
    Imath::M44f m((float)md[0][0], (float)md[0][1], (float)md[0][2],
                  (float)md[0][3], (float)md[1][0], (float)md[1][1],
                  (float)md[1][2], (float)md[1][3], (float)md[2][0],
                  (float)md[2][1], (float)md[2][2], (float)md[2][3],
                  (float)md[3][0], (float)md[3][1], (float)md[3][2],
                  (float)md[3][3]);

    // Map/scale the data window into a unit cube
    const Vec3f unitScale(1.0 / spec.full_width, 1.0 / spec.full_height,
                          1.0 / spec.full_depth);

    // Shift by min data window and half a voxel
    const Vec3f voxSize    = grid.voxelSize();
    const Vec3f dataOffset = (Vec3f(-spec.full_x, -spec.full_y, -spec.full_z)
                              * voxSize)
                             + (voxSize * 0.5);

    // Shift by the data offset
    m = Imath::M44f(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
                    dataOffset[0], dataOffset[1], dataOffset[2], 1.0)
        *
        // And scale to a unit cube
        Imath::M44f(unitScale[0], 0.0, 0.0, 0.0, 0.0, unitScale[1], 0.0, 0.0,
                    0.0, 0.0, unitScale[2], 0.0, 0.0, 0.0, 0.0, 1.0)
        * m;

    spec.attribute("worldtolocal", TypeMatrix, &m);

    for (auto metaItr = grid.beginMeta(), metaEnd = grid.endMeta();
         metaItr != metaEnd; ++metaItr) {
        const std::string& name = metaItr->first;
        const auto value        = metaItr->second;
        const auto&& type       = value->typeName();

        // Ordering below by amount from a default VDB from houdini
        if (type == StringMetadata::staticTypeName()) {
            spec.attribute(mdPrefix(name),
                           static_cast<StringMetadata&>(*value).value());
        } else if (type == Vec3SMetadata::staticTypeName()) {
            const auto v = static_cast<Vec3SMetadata&>(*value).value();
            spec.attribute(mdPrefix(name), TypeVector, &v);
        } else if (type == Int64Metadata::staticTypeName()) {
            const auto v = static_cast<Int64Metadata&>(*value).value();
            spec.attribute(mdPrefix(name), TypeDesc::INT64, &v);
        } else if (type == BoolMetadata::staticTypeName()) {
            spec.attribute(mdPrefix(name),
                           static_cast<BoolMetadata&>(*value).value());
        } else if (type == FloatMetadata::staticTypeName()) {
            spec.attribute(mdPrefix(name),
                           static_cast<FloatMetadata&>(*value).value());
        }

        else if (type == Int32Metadata::staticTypeName()) {
            spec.attribute(mdPrefix(name),
                           static_cast<Int32Metadata&>(*value).value());
        } else if (type == DoubleMetadata::staticTypeName()) {
            const auto v = static_cast<DoubleMetadata&>(*value).value();
            spec.attribute(mdPrefix(name), TypeDesc::DOUBLE, &v);
        }

        else if (type == Vec3IMetadata::staticTypeName()) {
            const auto v = static_cast<Vec3IMetadata&>(*value).value();
            spec.attribute(mdPrefix(name),
                           TypeDesc(TypeDesc::INT, TypeDesc::VEC3), &v);
        } else if (type == Vec3DMetadata::staticTypeName()) {
            const auto v = static_cast<Vec3DMetadata&>(*value).value();
            spec.attribute(mdPrefix(name),
                           TypeDesc(TypeDesc::DOUBLE, TypeDesc::VEC3), &v);
        }

        else if (type == Vec2SMetadata::staticTypeName()) {
            const auto v = static_cast<Vec2SMetadata&>(*value).value();
            spec.attribute(mdPrefix(name),
                           TypeDesc(TypeDesc::FLOAT, TypeDesc::VEC2), &v);
        } else if (type == Vec2IMetadata::staticTypeName()) {
            const auto v = static_cast<Vec2IMetadata&>(*value).value();
            spec.attribute(mdPrefix(name),
                           TypeDesc(TypeDesc::INT, TypeDesc::VEC2), &v);
        } else if (type == Vec2DMetadata::staticTypeName()) {
            const auto v = static_cast<Vec2DMetadata&>(*value).value();
            spec.attribute(mdPrefix(name),
                           TypeDesc(TypeDesc::DOUBLE, TypeDesc::VEC2), &v);
        }

        else if (type == Mat4SMetadata::staticTypeName()) {
            const auto v = static_cast<Mat4SMetadata&>(*value).value();
            spec.attribute(mdPrefix(name), TypeMatrix44, &v);
        } else if (type == Mat4DMetadata::staticTypeName()) {
            const auto v = static_cast<Mat4DMetadata&>(*value).value();
            spec.attribute(mdPrefix(name),
                           TypeDesc(TypeDesc::DOUBLE, TypeDesc::MATRIX44), &v);
        }
    }
}



bool
OpenVDBInput::open(const std::string& filename, ImageSpec& newspec)
{
    close();  // Reset any prior state; open() may be called more than once

    auto file = openVDB(filename, this);
    if (!file)
        return false;

    // The grid bounds can come from file metadata that nothing has vetted,
    // so the resulting spec is run through check_open() before any caller
    // gets a chance to size an allocation from it. The absolute limits it
    // applies (including "limits:imagesize_MB") are the guard here. There is
    // deliberately no check_compression_ratio() call: VDB is a sparse format
    // and a single constant-value tile can legitimately describe a huge dense
    // region from a tiny file, so no declared:file size ratio distinguishes a
    // genuine volume from a hostile one.
    const ROI range(0, 1 << 20, 0, 1 << 20, 0, 1 << 20, 0, 4);

    try {
        for (io::File::NameIterator name = file->beginName(),
                                    end  = file->endName();
             name != end; ++name) {
            std::string gridName  = name.gridName();
            GridBase::Ptr gridPtr = file->readGrid(gridName, BBoxd());
            if (!gridPtr) {
                init();  // Reset to initial state
                errorfmt("Could not read grid '{}' of '{}'", gridName,
                         filename);
                return false;
            }
            const CoordBBox bounds = getBoundingBox(*gridPtr);
            if (bounds.empty())
                continue;  // no representable extent; skip this grid

            ImageSpec spec;
            bool specok = false;
            ScalarGrid::Ptr fPtr;
            Vec3fGrid::Ptr v3Ptr;
            if ((fPtr = gridPtrCast<ScalarGrid>(gridPtr))) {
                spec   = ImageSpec(1, 1, 1, TypeFloat);
                specok = VDBReader<ScalarGrid>::fillSpec(bounds, spec);
            } else if ((v3Ptr = gridPtrCast<Vec3fGrid>(gridPtr))) {
                spec   = ImageSpec(1, 1, 3, TypeFloat);
                specok = VDBReader<Vec3fGrid>::fillSpec(bounds, spec);
            } else
                continue;
            if (!specok) {
                init();  // Reset to initial state
                errorfmt(
                    "Grid '{}' of '{}' has an out-of-range bounding box [{},{},{}]-[{},{},{}]. Possible corrupt input?",
                    gridName, filename, bounds.min().x(), bounds.min().y(),
                    bounds.min().z(), bounds.max().x(), bounds.max().y(),
                    bounds.max().z());
                return false;
            }
            if (!check_open(spec, range)) {
                init();  // Reset to initial state
                return false;
            }

            // gridName will now be moved/invalid
            m_layers.emplace_back(std::move(gridName), gridPtr->getName(),
                                  bounds, spec, std::move(gridPtr));

            auto& layer        = m_layers.back();
            auto& layerspec    = layer.spec;
            auto& channelnames = layerspec.channelnames;

            channelnames.resize(layerspec.nchannels);
            if (layerspec.nchannels > 1) {
                OIIO_DASSERT(layerspec.nchannels <= 4);
                const bool iscolor = layer.name == "Cd"
                                     || layer.name == "color";
                const char kChanName[4]
                    = { iscolor ? 'r' : 'x', iscolor ? 'g' : 'y',
                        iscolor ? 'b' : 'z', iscolor ? 'a' : 'w' };
                for (int c = 0; c < layerspec.nchannels; ++c)
                    channelnames[c] = layer.name + "."
                                      + std::string(&kChanName[c], 1);
            } else
                channelnames.back() = layer.name;

            readMetaData(*layer.grid, layer, layerspec);
        }
    } catch (const std::exception& e) {
        close();  // Reset to initial state
        errorfmt("Could not open '{}': {}", filename, clamped_what(e));
        return false;
    } catch (...) {
        close();  // Reset to initial state
        errorfmt("Could not open '{}': unknown exception thrown", filename);
        return false;
    }
    if (m_layers.empty()) {
        close();  // Reset to initial state
        errorfmt("'{}' contains no readable grids", filename);
        return false;
    }
    m_name       = filename;
    m_nsubimages = (int)m_layers.size();

    for (auto& lr : m_layers)
        lr.spec.attribute("oiio:subimages", m_nsubimages);

    bool ok = seek_subimage(0, 0);
    newspec = ImageInput::spec();
    if (!ok)
        close();
    return ok;
}



bool
OpenVDBInput::read_native_scanline(int /*subimage*/, int /*miplevel*/,
                                   int /*y*/, int /*z*/, void* /*data*/)
{
    errorfmt("openvdb images are tiled; scanline reads are not supported");
    return false;
}



bool
OpenVDBInput::read_native_tile(int subimage, int miplevel, int x, int y, int z,
                               void* data)
{
    OIIO_PRAGMA_WARNING_PUSH
#if OIIO_GNUC_VERSION >= 120100
    OIIO_GCC_ONLY_PRAGMA(GCC diagnostic ignored "-Wstringop-overflow")
#endif
    lock_guard lock(*this);
    if (!seek_subimage_nolock(subimage, miplevel))
        return false;

    // The tree walk below runs on data decoded by OpenVDB from an untrusted
    // file; anything it throws has to become a clean error rather than
    // escape through the ImageInput API.
    const layerrecord& lay = m_layers[m_subimage];
    try {
        switch (lay.spec.nchannels) {
        case 1:
            if (auto grid = gridPtrCast<ScalarGrid>(lay.grid)) {
                auto* values = reinterpret_cast<float*>(data);
                return VDBReader<FloatGrid>::readTile(*grid, x, y, z, values);
            }
            break;
        case 3:
            if (auto grid = gridPtrCast<Vec3fGrid>(lay.grid)) {
                auto* values = reinterpret_cast<Vec3f*>(data);
                return VDBReader<Vec3fGrid>::readTile(*grid, x, y, z, values);
            }
            break;
        default: break;
        }
    } catch (const std::exception& e) {
        errorfmt("Could not read tile {},{},{} of '{}': {}", x, y, z, m_name,
                 clamped_what(e));
        return false;
    } catch (...) {
        errorfmt("Could not read tile {},{},{} of '{}': unknown exception", x,
                 y, z, m_name);
        return false;
    }
    errorfmt("Could not read tile {},{},{} of '{}'", x, y, z, m_name);
    return false;
    OIIO_PRAGMA_WARNING_POP
}



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
openvdb_input_imageio_create()
{
    return new OpenVDBInput;
}

OIIO_EXPORT const char* openvdb_input_extensions[] = { "vdb", nullptr };

OIIO_EXPORT int openvdb_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
openvdb_imageio_library_version()
{
    return "OpenVDB " OPENVDB_LIBRARY_ABI_VERSION_STRING;
}

OIIO_PLUGIN_EXPORTS_END

OIIO_PLUGIN_NAMESPACE_END
