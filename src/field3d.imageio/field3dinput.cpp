/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/filesystem.h>

#include <OpenEXR/ImathVec.h>

#include "field3d_pvt.h"
using namespace OIIO_NAMESPACE::f3dpvt;


OIIO_PLUGIN_NAMESPACE_BEGIN

spin_mutex &
f3dpvt::field3d_mutex ()
{
    static spin_mutex m;
    return m;
}



class Field3DInput : public Field3DInput_Interface {
public:
    Field3DInput () { init(); }
    virtual ~Field3DInput () { close(); }
    virtual const char * format_name (void) const { return "field3d"; }
    virtual int supports (string_view feature) const {
        return (feature == "arbitrary_metadata");
    }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

    /// Transform a world space position to local coordinates, using the
    /// mapping of the current subimage.
    virtual void worldToLocal (const Imath::V3f &wsP, Imath::V3f &lsP,
                               float time) const;

private:
    std::string m_name;
    Field3DInputFile *m_input;
    int m_subimage;                 ///< What subimage/field are we looking at?
    int m_nsubimages;               ///< How many fields in the file?
    std::vector<layerrecord> m_layers;
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    template<typename T> void read_layers (TypeDesc datatype);

    void read_one_layer (FieldRes::Ptr field, layerrecord &lay,
                         TypeDesc datatype, size_t layernum);

    template<typename T> bool readtile (int x, int y, int z, T *data);

    void init () {
        m_name.clear ();
        m_input = NULL;
        m_subimage = -1;
        m_nsubimages = 0;
        m_layers.clear ();
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput *
field3d_input_imageio_create ()
{
    return new Field3DInput;
}

// OIIO_EXPORT int field3d_imageio_version = OIIO_PLUGIN_VERSION; // it's in field3doutput.cpp

OIIO_EXPORT const char * field3d_input_extensions[] = {
    "f3d", NULL
};

OIIO_PLUGIN_EXPORTS_END



void
f3dpvt::oiio_field3d_initialize ()
{
    static volatile bool initialized = false;

    if (! initialized) {
        spin_lock lock (field3d_mutex());
        if (! initialized) {
            initIO ();
            // Minimize Field3D's own internal caching
            SparseFileManager::singleton().setLimitMemUse(true); // Enables cache
            SparseFileManager::singleton().setMaxMemUse(20.0f); // In MB
#if (100*FIELD3D_MAJOR_VER + FIELD3D_MINOR_VER) >= 104
            Msg::setVerbosity (0); // Turn off console messages from F3D
#endif
            initialized = true;
        }
    }
}



template<typename T>
inline int blocksize (FieldRes::Ptr &f)
{
    ASSERT (f && "taking blocksize of null ptr");
    typename SparseField<T>::Ptr sf (field_dynamic_cast<SparseField<T> >(f));
    if (sf)
        return sf->blockSize();
    typename SparseField<T>::Ptr vsf (field_dynamic_cast<SparseField<T> >(f));
    if (vsf)
        return vsf->blockSize();
    return 0;
}



template <class M>
static void
read_metadata (const M &meta, ImageSpec &spec)
{
    for (typename M::StrMetadata::const_iterator i = meta.strMetadata().begin(),
             e = meta.strMetadata().end(); i != e;  ++i)
        spec.attribute (i->first, i->second);
    for (typename M::IntMetadata::const_iterator i = meta.intMetadata().begin(),
             e = meta.intMetadata().end(); i != e;  ++i)
        spec.attribute (i->first, i->second);
    for (typename M::FloatMetadata::const_iterator i = meta.floatMetadata().begin(),
             e = meta.floatMetadata().end(); i != e;  ++i)
        spec.attribute (i->first, i->second);
    for (typename M::VecIntMetadata::const_iterator i = meta.vecIntMetadata().begin(),
             e = meta.vecIntMetadata().end(); i != e;  ++i) {
        spec.attribute (i->first, TypeDesc(TypeDesc::INT,3), &(i->second));
    }
    for (typename M::VecFloatMetadata::const_iterator i = meta.vecFloatMetadata().begin(),
             e = meta.vecFloatMetadata().end(); i != e;  ++i) {
        spec.attribute (i->first, TypeVector, &(i->second));
    }
}



void
Field3DInput::read_one_layer (FieldRes::Ptr field, layerrecord &lay,
                              TypeDesc datatype, size_t layernum)
{
    lay.name = field->name;
    lay.attribute = field->attribute;
    lay.datatype = datatype;
    lay.extents = field->extents();
    lay.dataWindow = field->dataWindow();
    lay.field = field;

    // Generate a unique name for the layer.  Field3D files can have
    // multiple partitions (aka fields) with the same name, and
    // different partitions can each have attributes (aka layers) with
    // identical names.  The convention is that if there are duplicates,
    // insert a number to disambiguate.
    int duplicates = 0;
    for (int i = 0;  i < (int)layernum;  ++i)
        if (m_layers[i].name == lay.name && m_layers[i].attribute == lay.attribute)
            ++duplicates;
    if (! duplicates && lay.name == lay.attribute)
        lay.unique_name = lay.name;
    else
        lay.unique_name = duplicates
            ? Strutil::format ("%s.%u:%s", lay.name, duplicates+1, lay.attribute)
            : Strutil::format ("%s:%s", lay.name, lay.attribute);

    lay.spec = ImageSpec(); // Clear everything with default constructor
    lay.spec.format = lay.datatype;
    if (lay.vecfield) {
        lay.spec.nchannels = 3;
        lay.spec.channelnames.push_back (lay.attribute + ".x");
        lay.spec.channelnames.push_back (lay.attribute + ".y");
        lay.spec.channelnames.push_back (lay.attribute + ".z");
    } else {
        lay.spec.channelnames.push_back (lay.attribute);
        lay.spec.nchannels = 1;
    }

    lay.spec.x = lay.dataWindow.min.x;
    lay.spec.y = lay.dataWindow.min.y;
    lay.spec.z = lay.dataWindow.min.z;
    lay.spec.width = lay.dataWindow.max.x - lay.dataWindow.min.x + 1;
    lay.spec.height = lay.dataWindow.max.y - lay.dataWindow.min.y + 1;
    lay.spec.depth = lay.dataWindow.max.z - lay.dataWindow.min.z + 1;
    lay.spec.full_x = lay.extents.min.x;
    lay.spec.full_y = lay.extents.min.y;
    lay.spec.full_z = lay.extents.min.z;
    lay.spec.full_width  = lay.extents.max.x - lay.extents.min.x + 1;
    lay.spec.full_height = lay.extents.max.y - lay.extents.min.y + 1;
    lay.spec.full_depth  = lay.extents.max.z - lay.extents.min.z + 1;

    // Always appear tiled
    int b = 0;
    if (lay.fieldtype == f3dpvt::Sparse) {
        if (datatype == TypeDesc::FLOAT)
            b = blocksize<float>(field);
        else if (datatype == TypeDesc::HALF)
            b = blocksize<FIELD3D_NS::half>(field);
        else if (datatype == TypeDesc::DOUBLE)
            b = blocksize<double>(field);
    }
    if (b) {
        // There was a block size found
        lay.spec.tile_width = b;
        lay.spec.tile_height = b;
        lay.spec.tile_depth = b;
    } else {
        // Make the tiles be the volume size
        lay.spec.tile_width = lay.spec.width;
        lay.spec.tile_height = lay.spec.height;
        lay.spec.tile_depth = lay.spec.depth;
    }
    ASSERT (lay.spec.tile_width > 0 && lay.spec.tile_height > 0 &&
            lay.spec.tile_depth > 0);

    lay.spec.attribute ("ImageDescription", lay.unique_name);
    lay.spec.attribute ("oiio:subimagename", lay.unique_name);
    lay.spec.attribute ("field3d:partition", lay.name);
    lay.spec.attribute ("field3d:layer", lay.attribute);
    lay.spec.attribute ("field3d:fieldtype", field->className());

    FieldMapping::Ptr mapping = field->mapping();
    lay.spec.attribute ("field3d:mapping", mapping->className());
    MatrixFieldMapping::Ptr matrixMapping = 
        boost::dynamic_pointer_cast<MatrixFieldMapping>(mapping);
    if (matrixMapping) {
        Imath::M44d md = matrixMapping->localToWorld();
        lay.spec.attribute ("field3d:localtoworld",
                        TypeDesc(TypeDesc::DOUBLE, TypeDesc::MATRIX44), &md);
        Imath::M44f m ((float)md[0][0], (float)md[0][1], (float)md[0][2], (float)md[0][3], 
                       (float)md[1][0], (float)md[1][1], (float)md[1][2], (float)md[1][3],
                       (float)md[2][0], (float)md[2][1], (float)md[2][2], (float)md[2][3],
                       (float)md[3][0], (float)md[3][1], (float)md[3][2], (float)md[3][3]);
        m = m.inverse();
        lay.spec.attribute ("worldtocamera", TypeMatrix, &m);
    }

    // Other metadata
    read_metadata (m_input->metadata(), lay.spec);  // global
    read_metadata (field->metadata(), lay.spec);    // specific to this field
}



/// Read all layers from the open file that match the data type.
/// Find the list of scalar and vector fields.
template <typename Data_T>
void Field3DInput::read_layers (TypeDesc datatype)
{
    typedef typename Field<Data_T>::Vec SFieldList;
    SFieldList sFields = m_input->readScalarLayers<Data_T>();
    if (sFields.size() > 0) {
        for (typename SFieldList::const_iterator i = sFields.begin(); 
             i != sFields.end(); ++i) {
            size_t layernum = m_layers.size();
            m_layers.resize (layernum+1);
            layerrecord &lay (m_layers.back());
            if (field_dynamic_cast<DenseField<Data_T> >(*i))
                lay.fieldtype = f3dpvt::Dense;
            else if (field_dynamic_cast<SparseField<Data_T> >(*i))
                lay.fieldtype = f3dpvt::Sparse;
            else
                ASSERT (0 && "unknown field type");
            read_one_layer (*i, lay, datatype, layernum);
        }
    }

    // Note that both scalar and vector calls take the scalar type as argument
    typedef typename Field<FIELD3D_VEC3_T<Data_T> >::Vec VFieldList;
    VFieldList vFields = m_input->readVectorLayers<Data_T>();
    if (vFields.size() > 0) {
        for (typename VFieldList::const_iterator i = vFields.begin(); 
             i != vFields.end(); ++i) {
            size_t layernum = m_layers.size();
            m_layers.resize (layernum+1);
            layerrecord &lay (m_layers.back());
            typedef FIELD3D_VEC3_T<Data_T> VecData_T;
            if (field_dynamic_cast<DenseField<VecData_T> >(*i))
                lay.fieldtype = f3dpvt::Dense;
            else if (field_dynamic_cast<SparseField<VecData_T> >(*i))
                lay.fieldtype = f3dpvt::Sparse;
            else if (field_dynamic_cast<MACField<VecData_T> >(*i))
                lay.fieldtype = f3dpvt::MAC;
            else
                ASSERT (0 && "unknown field type");
            lay.vecfield = true;
            read_one_layer (*i, lay, datatype, layernum);
        }
    }
}



bool
Field3DInput::valid_file (const std::string &filename) const
{
    if (! Filesystem::is_regular (filename))
        return false;

    // The f3d is flaky when opening some non-f3d files. It should just fail
    // gracefully, but it doesn't always. So to keep my sanity, don't even
    // bother trying for filenames that don't end in .f3d.
    if (! Strutil::iends_with (filename, ".f3d"))
        return false;

    oiio_field3d_initialize ();

    bool ok = false;
    Field3DInputFile *input = NULL;
    {
        spin_lock lock (field3d_mutex());
        input = new Field3DInputFile;
        try {
            ok = input->open (filename);
        } catch (...) {
            ok = false;
        }
        delete input;
    }

    return ok;
}



bool
Field3DInput::open (const std::string &name, ImageSpec &newspec)
{
    if (m_input)
        close();

    if (! Filesystem::is_regular (name))
        return false;

    // The f3d is flaky when opening some non-f3d files. It should just fail
    // gracefully, but it doesn't always. So to keep my sanity, don't even
    // bother trying for filenames that don't end in .f3d.
    if (! Strutil::iends_with (name, ".f3d"))
        return false;

    oiio_field3d_initialize ();

    {
        spin_lock lock (field3d_mutex());
        m_input = new Field3DInputFile;
        bool ok = false;
        try {
            ok = m_input->open (name);
        } catch (...) {
            ok = false;
        }
        if (! ok) {
            delete m_input;
            m_input = NULL;
            m_name.clear ();
            return false;
        }
        m_name = name;

        std::vector<std::string> partitions;
        m_input->getPartitionNames (partitions);

        // There's no apparent way to loop over all fields and layers -- the
        // Field3D library is templated so that it's most natural to have
        // the "outer loop" be the data type.  So for each type, we augment
        // our list of layers with the matching ones in the file.
        read_layers<FIELD3D_NS::half> (TypeDesc::HALF);
        read_layers<float> (TypeDesc::FLOAT);
        read_layers<double> (TypeDesc::DOUBLE);
    }

    m_nsubimages = (int) m_layers.size();
    return seek_subimage (0, 0, newspec);
}



bool
Field3DInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    if (subimage < 0 || subimage >= m_nsubimages)   // out of range
        return false;
    if (miplevel != 0)
        return false;

    m_subimage = subimage;
    m_spec = m_layers[subimage].spec;
    newspec = m_spec;
    return true;
}



bool
Field3DInput::close ()
{
    spin_lock lock (field3d_mutex());
    if (m_input) {
        m_input->close ();
        delete m_input;   // implicity closes
        m_input = NULL;
        m_name.clear ();
    }

    init ();  // Reset to initial state
    return true;
}



bool
Field3DInput::read_native_scanline (int y, int z, void *data)
{
    // scanlines not supported
    return false;
}



template<class T>
bool Field3DInput::readtile (int x, int y, int z, T *data)
{
    layerrecord &lay (m_layers[m_subimage]);
    int xend = std::min (x+lay.spec.tile_width, lay.spec.x+lay.spec.width);
    int yend = std::min (y+lay.spec.tile_height, lay.spec.y+lay.spec.height);
    int zend = std::min (z+lay.spec.tile_depth, lay.spec.z+lay.spec.depth); 
    {
        typename DenseField<T>::Ptr f = field_dynamic_cast<DenseField<T> > (lay.field);
        if (f) {
            //std::cerr << "readtile dense " << x << '-' << xend << " x " 
            //        <<  y << '-' << yend << " x " << z << '-' << zend << "\n";
            for (int k = z; k < zend; ++k) {
                for (int j = y; j < yend; ++j) {
                    T *d = data + (k-z)*(lay.spec.tile_width*lay.spec.tile_height)
                                + (j-y)*lay.spec.tile_width;
                    for (int i = x; i < xend; ++i, ++d) {
                        *d = f->fastValue (i, j, k);
                    }
                }
            }
            return true;
        }
    }
    {
        typename SparseField<T>::Ptr f = field_dynamic_cast<SparseField<T> > (lay.field);
        if (f) {
            //std::cerr << "readtile sparse " << x << '-' << xend << " x " 
            //        <<  y << '-' << yend << " x " << z << '-' << zend << "\n";
            for (int k = z; k < zend; ++k) {
                for (int j = y; j < yend; ++j) {
                    T *d = data + (k-z)*(lay.spec.tile_width*lay.spec.tile_height)
                                + (j-y)*lay.spec.tile_width;
                    for (int i = x; i < xend; ++i, ++d) {
                        *d = f->fastValue (i, j, k);
                    }
                }
            }
            return true;
        }
    }
    return false;
}



bool
Field3DInput::read_native_tile (int x, int y, int z, void *data)
{
    spin_lock lock (field3d_mutex());
    layerrecord &lay (m_layers[m_subimage]);
    if (lay.datatype == TypeDesc::FLOAT) {
        if (lay.vecfield)
            return readtile (x, y, z, (FIELD3D_VEC3_T<float> *)data);
        else
            return readtile (x, y, z, (float *)data);
    } else if (lay.datatype == TypeDesc::HALF) {
        if (lay.vecfield)
            return readtile (x, y, z, (FIELD3D_VEC3_T<FIELD3D_NS::half> *)data);
        else
            return readtile (x, y, z, (FIELD3D_NS::half *)data);
    } else if (lay.datatype == TypeDesc::DOUBLE) {
        if (lay.vecfield)
            return readtile (x, y, z, (FIELD3D_VEC3_T<double> *)data);
        else
            return readtile (x, y, z, (double *)data);
    }

    return false;
}



void
Field3DInput::worldToLocal (const Imath::V3f &wsP, Imath::V3f &lsP,
                            float time) const
{
    spin_lock lock (field3d_mutex());
    const layerrecord &lay (m_layers[m_subimage]);
    V3d Pw (wsP[0], wsP[1], wsP[2]);
    V3d Pl;
    lay.field->mapping()->worldToLocal(Pw, Pl, time);
    lsP[0] = (float) Pl[0];
    lsP[1] = (float) Pl[1];
    lsP[2] = (float) Pl[2];
}


OIIO_PLUGIN_NAMESPACE_END

