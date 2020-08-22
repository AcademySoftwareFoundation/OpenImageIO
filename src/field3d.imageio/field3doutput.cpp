// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>

#include "field3d_pvt.h"
using namespace OIIO_NAMESPACE::f3dpvt;



OIIO_PLUGIN_NAMESPACE_BEGIN



class Field3DOutput final : public ImageOutput {
public:
    Field3DOutput();
    virtual ~Field3DOutput();
    virtual const char* format_name(void) const override { return "field3d"; }
    virtual int supports(string_view feature) const override;
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode) override;
    virtual bool open(const std::string& name, int subimages,
                      const ImageSpec* specs) override;
    virtual bool close() override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;

private:
    std::string m_name;
    Field3DOutputFile* m_output;
    int m_subimage;       ///< What subimage/field are we writing now
    int m_nsubimages;     ///< How many subimages will be in the file?
    bool m_writepending;  ///< Is there an unwritten current layer?
    std::vector<ImageSpec> m_specs;
    std::vector<unsigned char> m_scratch;  ///< Scratch space for us to use
    FieldRes::Ptr m_field;

    // Initialize private members to pre-opened state
    void init(void)
    {
        m_name.clear();
        m_output     = NULL;
        m_subimage   = -1;
        m_nsubimages = 0;
        m_specs.clear();
        m_writepending = false;
    }

    // Add a parameter to the output
    bool put_parameter(const std::string& name, TypeDesc type,
                       const void* data);

    bool prep_subimage();
    bool write_current_subimage();
    template<typename T> bool prep_subimage_specialized();
    template<typename T> bool write_current_subimage_specialized();
    template<typename T> bool write_current_subimage_specialized_vec();
    template<typename T>
    bool write_scanline_specialized(int y, int z, const T* data);
    template<typename T>
    bool write_tile_specialized(int x, int y, int z, const T* data);
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
field3d_output_imageio_create()
{
    return new Field3DOutput;
}

OIIO_EXPORT int field3d_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
field3d_imageio_library_version()
{
    return ustring::sprintf("Field3d %d.%d.%d", FIELD3D_MAJOR_VER,
                            FIELD3D_MINOR_VER, FIELD3D_MICRO_VER)
        .c_str();
}

OIIO_EXPORT const char* field3d_output_extensions[] = { "f3d", nullptr };

OIIO_PLUGIN_EXPORTS_END



namespace {  // anon namespace

// format-specific metadata prefixes
static std::vector<std::string> format_prefixes;
static atomic_int format_prefixes_initialized;
static spin_mutex format_prefixes_mutex;  // guard

}  // namespace



Field3DOutput::Field3DOutput() { init(); }



Field3DOutput::~Field3DOutput()
{
    // Close, if not already done.
    close();
}



int
Field3DOutput::supports(string_view feature) const
{
    return (feature == "tiles" || feature == "multiimage"
            || feature == "random_access" || feature == "arbitrary_metadata"
            || feature == "exif"    // Because of arbitrary_metadata
            || feature == "iptc");  // Because of arbitrary_metadata

    // FIXME: we could support "empty"
    // FIXME: newer releases of Field3D support mipmap
}



bool
Field3DOutput::open(const std::string& name, const ImageSpec& userspec,
                    OpenMode mode)
{
    // If called the old-fashioned way, for one subimage, just turn it into
    // a call to the multi-subimage open() with a single subimage.
    if (mode == Create)
        return open(name, 1, &userspec);

    if (mode == AppendMIPLevel) {
        errorf("%s does not support MIP-mapping", format_name());
        return false;
    }

    OIIO_ASSERT(mode == AppendSubimage && "invalid open() mode");

    write_current_subimage();

    ++m_subimage;
    if (m_subimage >= m_nsubimages) {
        errorf("Appending past the pre-declared number of subimages (%d)",
               m_nsubimages);
        return false;
    }

    if (!prep_subimage())
        return false;

    return true;
}



bool
Field3DOutput::open(const std::string& name, int subimages,
                    const ImageSpec* specs)
{
    if (m_output)
        close();

    if (subimages < 1) {
        errorf("%s does not support %d subimages.", format_name(), subimages);
        return false;
    }

    oiio_field3d_initialize();

    m_nsubimages = subimages;
    m_subimage   = 0;

    {
        spin_lock lock(field3d_mutex());
        m_output = new Field3DOutputFile;
        bool ok  = false;
        try {
            ok = m_output->create(name);
        } catch (...) {
            ok = false;
        }
        if (!ok) {
            delete m_output;
            m_output = NULL;
            m_name.clear();
            return false;
        }
        m_name = name;
    }

    m_specs.assign(specs, specs + subimages);
    for (int s = 0; s < m_nsubimages; ++s) {
        ImageSpec& spec(m_specs[s]);
        if (spec.format != TypeDesc::HALF && spec.format != TypeDesc::DOUBLE) {
            spec.format = TypeDesc::FLOAT;
        }
        if (spec.nchannels != 1 && spec.nchannels != 3) {
            errorf("%s does not allow %d channels in a field (subimage %d)",
                   format_name(), spec.nchannels, s);
            return false;
        }
    }

    if (!prep_subimage())  // get ready for first subimage
        return false;

    return true;
}



bool
Field3DOutput::put_parameter(const std::string& name, TypeDesc type,
                             const void* data)
{
    if (Strutil::istarts_with(name, "field3d:")
        || Strutil::istarts_with(name, "oiio:"))
        return false;  // skip these; handled separately or not at all

    // Before handling general named metadata, suppress non-openexr
    // format-specific metadata.
    if (const char* colon = strchr(name.c_str(), ':')) {
        std::string prefix(name.c_str(), colon);
        if (!Strutil::iequals(prefix, "openexr")) {
            if (!format_prefixes_initialized) {
                // Retrieve and split the list, only the first time
                spin_lock lock(format_prefixes_mutex);
                std::string format_list;
                OIIO::getattribute("format_list", format_list);
                Strutil::split(format_list, format_prefixes, ",");
                format_prefixes_initialized = true;
            }
            for (const auto& f : format_prefixes)
                if (Strutil::iequals(prefix, f))
                    return false;
        }
    }

    if (type == TypeString)
        m_field->metadata().setStrMetadata(name, *(const char**)data);
    else if (type == TypeInt)
        m_field->metadata().setIntMetadata(name, *(const int*)data);
    else if (type == TypeFloat)
        m_field->metadata().setFloatMetadata(name, *(const float*)data);
    else if (type.basetype == TypeDesc::FLOAT && type.aggregate == 3)
        m_field->metadata().setVecFloatMetadata(name,
                                                *(const FIELD3D_NS::V3f*)data);
    else if (type.basetype == TypeDesc::INT && type.aggregate == 3)
        m_field->metadata().setVecIntMetadata(name,
                                              *(const FIELD3D_NS::V3i*)data);
    else
        return false;

    return true;
}



bool
Field3DOutput::close()
{
    spin_lock lock(field3d_mutex());
    if (m_output) {
        write_current_subimage();
        m_output->close();
        delete m_output;  // implicitly closes
        m_output = NULL;
    }

    init();       // re-initialize
    return true;  // How can we fail?
}



template<typename T>
bool
Field3DOutput::write_scanline_specialized(int y, int z, const T* data)
{
    int xend = m_spec.x + m_spec.width;

    if (typename DenseField<T>::Ptr f = field_dynamic_cast<DenseField<T>>(
            m_field)) {
        for (int x = m_spec.x; x < xend; ++x)
            f->lvalue(x, y, z) = *data++;
        return true;
    }
    if (typename SparseField<T>::Ptr f = field_dynamic_cast<SparseField<T>>(
            m_field)) {
        for (int x = m_spec.x; x < xend; ++x)
            f->lvalue(x, y, z) = *data++;
        return true;
    }

    errorf("Unknown field type");
    return false;
}



bool
Field3DOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                              stride_t xstride)
{
    m_spec.auto_stride(xstride, format, spec().nchannels);
    data = to_native_scanline(format, data, xstride, m_scratch);

    if (m_spec.format == TypeDesc::FLOAT) {
        if (m_spec.nchannels == 1)
            return write_scanline_specialized(y, z, (const float*)data);
        else
            return write_scanline_specialized(
                y, z, (const FIELD3D_VEC3_T<float>*)data);
    } else if (m_spec.format == TypeDesc::DOUBLE) {
        if (m_spec.nchannels == 1)
            return write_scanline_specialized(y, z, (const double*)data);
        else
            return write_scanline_specialized(
                y, z, (const FIELD3D_VEC3_T<double>*)data);
    } else if (m_spec.format == TypeDesc::HALF) {
        if (m_spec.nchannels == 1)
            return write_scanline_specialized(y, z,
                                              (const FIELD3D_NS::half*)data);
        else
            return write_scanline_specialized(
                y, z, (const FIELD3D_VEC3_T<FIELD3D_NS::half>*)data);
    } else {
        OIIO_ASSERT(0 && "Unsupported data format for field3d");
    }

    return false;
}



template<typename T>
bool
Field3DOutput::write_tile_specialized(int x, int y, int z, const T* data)
{
    int xend = std::min(x + m_spec.tile_width, m_spec.x + m_spec.width);
    int yend = std::min(y + m_spec.tile_height, m_spec.y + m_spec.height);
    int zend = std::min(z + m_spec.tile_depth, m_spec.z + m_spec.depth);

    if (typename DenseField<T>::Ptr f = field_dynamic_cast<DenseField<T>>(
            m_field)) {
        for (int k = z; k < zend; ++k) {
            for (int j = y; j < yend; ++j) {
                const T* d
                    = data + (k - z) * (m_spec.tile_width * m_spec.tile_height)
                      + (j - y) * m_spec.tile_width;
                for (int i = x; i < xend; ++i)
                    f->lvalue(i, j, k) = *d++;
            }
        }
        return true;
    }

    if (typename SparseField<T>::Ptr f = field_dynamic_cast<SparseField<T>>(
            m_field)) {
        for (int k = z; k < zend; ++k) {
            for (int j = y; j < yend; ++j) {
                const T* d
                    = data + (k - z) * (m_spec.tile_width * m_spec.tile_height)
                      + (j - y) * m_spec.tile_width;
                for (int i = x; i < xend; ++i)
                    f->lvalue(i, j, k) = *d++;
            }
        }
        return true;
    }

    errorf("Unknown field type");
    return false;
}



bool
Field3DOutput::write_tile(int x, int y, int z, TypeDesc format,
                          const void* data, stride_t xstride, stride_t ystride,
                          stride_t zstride)
{
    m_spec.auto_stride(xstride, ystride, zstride, format, spec().nchannels,
                       spec().tile_width, spec().tile_height);
    data = to_native_tile(format, data, xstride, ystride, zstride, m_scratch);


    if (m_spec.format == TypeDesc::FLOAT) {
        if (m_spec.nchannels == 1)
            return write_tile_specialized(x, y, z, (const float*)data);
        else
            return write_tile_specialized(x, y, z,
                                          (const FIELD3D_VEC3_T<float>*)data);
    } else if (m_spec.format == TypeDesc::DOUBLE) {
        if (m_spec.nchannels == 1)
            return write_tile_specialized(x, y, z, (const double*)data);
        else
            return write_tile_specialized(x, y, z,
                                          (const FIELD3D_VEC3_T<double>*)data);
    } else if (m_spec.format == TypeDesc::HALF) {
        if (m_spec.nchannels == 1)
            return write_tile_specialized(x, y, z,
                                          (const FIELD3D_NS::half*)data);
        else
            return write_tile_specialized(
                x, y, z, (const FIELD3D_VEC3_T<FIELD3D_NS::half>*)data);
    } else {
        OIIO_ASSERT(0 && "Unsupported data format for field3d");
    }

    return false;
}



template<typename T>
bool
Field3DOutput::prep_subimage_specialized()
{
    m_spec = m_specs[m_subimage];
    OIIO_ASSERT(m_spec.nchannels == 1 || m_spec.nchannels == 3);

    Box3i extents(V3i(m_spec.full_x, m_spec.full_y, m_spec.full_z),
                  V3i(m_spec.full_x + m_spec.full_width - 1,
                      m_spec.full_y + m_spec.full_height - 1,
                      m_spec.full_z + m_spec.full_depth - 1));
    Box3i datawin(V3i(m_spec.x, m_spec.y, m_spec.z),
                  V3i(m_spec.x + m_spec.width - 1, m_spec.y + m_spec.height - 1,
                      m_spec.z + m_spec.depth - 1));

    std::string fieldtype = m_spec.get_string_attribute("field3d:fieldtype");
    if (Strutil::iequals(fieldtype, "SparseField")) {
        // Sparse
        SparseField<T>* f(new SparseField<T>);
        f->setSize(extents, datawin);
        m_field.reset(f);
    } else if (Strutil::iequals(fieldtype, "MAC")) {
        // FIXME
        OIIO_ASSERT(0 && "MAC fields not yet supported");
    } else {
        // Dense
        DenseField<T>* f(new DenseField<T>);
        f->setSize(extents, datawin);
        m_field.reset(f);
    }

    std::string name      = m_spec.get_string_attribute("field3d:partition");
    std::string attribute = m_spec.get_string_attribute("field3d:layer");
    if (!name.size() && !attribute.size()) {
        // Try to extract from the subimagename or if that fails,
        // ImageDescription
        std::string unique_name = m_spec.get_string_attribute(
            "oiio:subimagename");
        if (unique_name.size() == 0)
            unique_name = m_spec.get_string_attribute("ImageDescription");
        if (unique_name.size() == 0)
            unique_name = "name:attribute";  // punt
        std::vector<std::string> pieces;
        Strutil::split(unique_name, pieces);
        if (pieces.size() > 0)
            name = pieces[0];
        if (pieces.size() > 1)
            attribute = pieces[1];
    }

    m_field->name      = name;
    m_field->attribute = attribute;

    // Mapping matrix
    TypeDesc TypeMatrixD(TypeDesc::DOUBLE, TypeDesc::MATRIX44);
    if (ParamValue* mx = m_spec.find_attribute("field3d:localtoworld",
                                               TypeMatrixD)) {
        MatrixFieldMapping::Ptr mapping(new MatrixFieldMapping);
        mapping->setLocalToWorld(*((FIELD3D_NS::M44d*)mx->data()));
        m_field->setMapping(mapping);
    } else if (ParamValue* mx = m_spec.find_attribute("worldtocamera",
                                                      TypeMatrix)) {
        Imath::M44f m = *((Imath::M44f*)mx->data());
        m             = m.inverse();
        FIELD3D_NS::M44d md(m[0][0], m[0][1], m[0][1], m[0][3], m[1][0],
                            m[1][1], m[1][1], m[1][3], m[2][0], m[2][1],
                            m[2][1], m[2][3], m[3][0], m[3][1], m[3][1],
                            m[3][3]);
        MatrixFieldMapping::Ptr mapping(new MatrixFieldMapping);
        mapping->setLocalToWorld(md);
        m_field->setMapping(mapping);
    }

    // Miscellaneous metadata
    for (size_t p = 0; p < spec().extra_attribs.size(); ++p)
        put_parameter(spec().extra_attribs[p].name().string(),
                      spec().extra_attribs[p].type(),
                      spec().extra_attribs[p].data());

    return true;
}



bool
Field3DOutput::prep_subimage()
{
    m_spec = m_specs[m_subimage];
    OIIO_ASSERT(m_spec.nchannels == 1 || m_spec.nchannels == 3);
    if (m_spec.format == TypeDesc::FLOAT) {
        if (m_spec.nchannels == 1)
            prep_subimage_specialized<float>();
        else
            prep_subimage_specialized<FIELD3D_VEC3_T<float>>();
    } else if (m_spec.format == TypeDesc::DOUBLE) {
        if (m_spec.nchannels == 1)
            prep_subimage_specialized<double>();
        else
            prep_subimage_specialized<FIELD3D_VEC3_T<double>>();
    } else if (m_spec.format == TypeDesc::HALF) {
        if (m_spec.nchannels == 1)
            prep_subimage_specialized<FIELD3D_NS::half>();
        else
            prep_subimage_specialized<FIELD3D_VEC3_T<FIELD3D_NS::half>>();
    } else {
        OIIO_ASSERT(0 && "Unsupported data format for field3d");
    }

    m_writepending = true;
    return true;
}



template<typename T>
bool
Field3DOutput::write_current_subimage_specialized()
{
    if (typename DenseField<T>::Ptr df = field_dynamic_cast<DenseField<T>>(
            m_field)) {
        m_output->writeScalarLayer<T>(df);
        return true;
    }

    if (typename SparseField<T>::Ptr sf = field_dynamic_cast<SparseField<T>>(
            m_field)) {
        m_output->writeScalarLayer<T>(sf);
        return true;
    }

    return false;
}



template<typename T>
bool
Field3DOutput::write_current_subimage_specialized_vec()
{
    typedef FIELD3D_VEC3_T<T> V;
    if (typename DenseField<V>::Ptr df = field_dynamic_cast<DenseField<V>>(
            m_field)) {
        m_output->writeVectorLayer<T>(df);
        return true;
    }

    if (typename SparseField<V>::Ptr sf = field_dynamic_cast<SparseField<V>>(
            m_field)) {
        m_output->writeVectorLayer<T>(sf);
        return true;
    }

    return false;
}



bool
Field3DOutput::write_current_subimage()
{
    if (!m_writepending)
        return true;

    bool ok = false;
    if (m_spec.format == TypeDesc::FLOAT) {
        if (m_spec.nchannels == 1)
            ok = write_current_subimage_specialized<float>();
        else
            ok = write_current_subimage_specialized_vec<float>();
    } else if (m_spec.format == TypeDesc::DOUBLE) {
        if (m_spec.nchannels == 1)
            ok = write_current_subimage_specialized<double>();
        else
            ok = write_current_subimage_specialized_vec<double>();
    } else if (m_spec.format == TypeDesc::HALF) {
        if (m_spec.nchannels == 1)
            ok = write_current_subimage_specialized<FIELD3D_NS::half>();
        else
            ok = write_current_subimage_specialized_vec<FIELD3D_NS::half>();
    }

    m_writepending = false;
    m_field.reset();
    return ok;
}


OIIO_PLUGIN_NAMESPACE_END
