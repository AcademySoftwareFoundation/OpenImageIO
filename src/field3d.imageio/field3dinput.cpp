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

#include "dassert.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"

#include <boost/foreach.hpp>

#include <Field3D/DenseField.h>
#include <Field3D/MACField.h>
#include <Field3D/SparseField.h>
#include <Field3D/InitIO.h>
#include <Field3D/Field3DFile.h>
#ifdef FIELD3D_NEW_API
#include <Field3D/FieldMetadata.h>
#endif
#ifndef FIELD3D_NS
#define FIELD3D_NS Field3D
#endif
using namespace FIELD3D_NS;

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace OpenImageIO;


namespace field3d_pvt {
mutex field3d_mutex;
};



class Field3DInput : public ImageInput {
public:
    Field3DInput () { init(); }
    virtual ~Field3DInput () { close(); }
    virtual const char * format_name (void) const { return "field3d"; }
    virtual bool open (const std::string &name, ImageSpec &newspec);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);

private:
    enum FieldType { Dense, Sparse, MAC };

    struct layerrecord {
        std::string name;
        std::string attribute;
        TypeDesc datatype;
        FieldType fieldtype;
        bool vecfield;      // true=vector, false=scalar
        Box3i extents;
        Box3i dataWindow;
        ImageSpec spec;
        FieldRes::Ptr field;

        layerrecord () : vecfield(false) { }
    };

    std::string m_name;
    Field3DInputFile *m_input;
    int m_subimage;                 ///< What subimage/field are we looking at?
    int m_nsubimages;               ///< How many fields in the file?
    std::vector<layerrecord> m_layers;
    std::vector<unsigned char> m_scratch; ///< Scratch space for us to use

    template<typename T> void read_layers (TypeDesc datatype);

    void read_one_layer (FieldRes::Ptr field, layerrecord &lay,
                         TypeDesc datatype);

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

DLLEXPORT ImageInput *
field3d_input_imageio_create ()
{
    return new Field3DInput;
}

// DLLEXPORT int field3d_imageio_version = OPENIMAGEIO_PLUGIN_VERSION; // it's in field3doutput.cpp

DLLEXPORT const char * field3d_input_extensions[] = {
    "f3d", NULL
};

OIIO_PLUGIN_EXPORTS_END



void
oiio_field3d_initialize ()
{
    static volatile bool initialized = false;
    static spin_mutex mutex;

    if (! initialized) {
        spin_lock lock (mutex);
        if (! initialized) {
            initIO ();
            // Minimize Field3D's own internal caching
            SparseFileManager::singleton().setLimitMemUse(true); // Enables cache
            SparseFileManager::singleton().setMaxMemUse(20.0f); // In MB
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
    typedef FIELD3D_VEC3_T<T> VecData_T;
    typename SparseField<T>::Ptr vsf (field_dynamic_cast<SparseField<T> >(f));
    if (vsf)
        return vsf->blockSize();
    return 0;
}



void
Field3DInput::read_one_layer (FieldRes::Ptr field, layerrecord &lay,
                              TypeDesc datatype)
{
    lay.name = field->name;
    lay.attribute = field->attribute;
    lay.datatype = datatype;
    lay.extents = field->extents();
    lay.dataWindow = field->dataWindow();
    lay.field = field;

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
    if (lay.fieldtype == Sparse) {
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

    lay.spec.attribute ("ImageDescription", lay.name + "." + lay.attribute);
    lay.spec.attribute ("field3d:name", lay.name);
    lay.spec.attribute ("field3d:attribute", lay.attribute);
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
        lay.spec.attribute ("worldtocamera", TypeDesc::TypeMatrix, &m);
    }

    // Other metadata
#ifdef FIELD3D_NEW_API
    // API for accessing metadata will be changing soon.  This will be
    // the new one.  Once it's pushed to the public Field3D, we'll
    // eliminate this #ifdef.
    for (FieldMetadata<FieldBase>::StrMetadata::const_iterator i = field->metadata().strMetadata().begin(),
             e = field->metadata().strMetadata().end(); i != e;  ++i)
        lay.spec.attribute (i->first, i->second);
    for (FieldMetadata<FieldBase>::IntMetadata::const_iterator i = field->metadata().intMetadata().begin(),
             e = field->metadata().intMetadata().end(); i != e;  ++i)
        lay.spec.attribute (i->first, i->second);
    for (FieldMetadata<FieldBase>::FloatMetadata::const_iterator i = field->metadata().floatMetadata().begin(),
             e = field->metadata().floatMetadata().end(); i != e;  ++i)
        lay.spec.attribute (i->first, i->second);
    for (FieldMetadata<FieldBase>::VecIntMetadata::const_iterator i = field->metadata().vecIntMetadata().begin(),
             e = field->metadata().vecIntMetadata().end(); i != e;  ++i) {
        lay.spec.attribute (i->first, TypeDesc(TypeDesc::INT,3), &(i->second));
    }
    for (FieldMetadata<FieldBase>::VecFloatMetadata::const_iterator i = field->metadata().vecFloatMetadata().begin(),
             e = field->metadata().vecFloatMetadata().end(); i != e;  ++i) {
        lay.spec.attribute (i->first, TypeDesc::TypeVector, &(i->second));
    }
#else
    for (FieldBase::StrMetadata::const_iterator i = field->strMetadata().begin(),
             e = field->strMetadata().end(); i != e;  ++i)
        lay.spec.attribute (i->first, i->second);
    for (FieldBase::IntMetadata::const_iterator i = field->intMetadata().begin(),
             e = field->intMetadata().end(); i != e;  ++i)
        lay.spec.attribute (i->first, i->second);
    for (FieldBase::FloatMetadata::const_iterator i = field->floatMetadata().begin(),
             e = field->floatMetadata().end(); i != e;  ++i)
        lay.spec.attribute (i->first, i->second);
    for (FieldBase::VecIntMetadata::const_iterator i = field->vecIntMetadata().begin(),
             e = field->vecIntMetadata().end(); i != e;  ++i) {
        lay.spec.attribute (i->first, TypeDesc(TypeDesc::INT,3), &(i->second));
    }
    for (FieldBase::VecFloatMetadata::const_iterator i = field->vecFloatMetadata().begin(),
             e = field->vecFloatMetadata().end(); i != e;  ++i) {
        lay.spec.attribute (i->first, TypeDesc::TypeVector, &(i->second));
    }
#endif

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
            m_layers.resize (m_layers.size()+1);
            layerrecord &lay (m_layers.back());
            if (field_dynamic_cast<DenseField<Data_T> >(*i))
                lay.fieldtype = Dense;
            else if (field_dynamic_cast<SparseField<Data_T> >(*i))
                lay.fieldtype = Sparse;
            else
                ASSERT (0 && "unknown field type");
            read_one_layer (*i, lay, datatype);
        }
    }

    // Note that both scalar and vector calls take the scalar type as argument
    typedef typename Field<FIELD3D_VEC3_T<Data_T> >::Vec VFieldList;
    VFieldList vFields = m_input->readVectorLayers<Data_T>();
    if (vFields.size() > 0) {
        for (typename VFieldList::const_iterator i = vFields.begin(); 
             i != vFields.end(); ++i) {
            m_layers.resize (m_layers.size()+1);
            layerrecord &lay (m_layers.back());
            typedef FIELD3D_VEC3_T<Data_T> VecData_T;
            if (field_dynamic_cast<DenseField<VecData_T> >(*i))
                lay.fieldtype = Dense;
            else if (field_dynamic_cast<SparseField<VecData_T> >(*i))
                lay.fieldtype = Sparse;
            else if (field_dynamic_cast<MACField<VecData_T> >(*i))
                lay.fieldtype = MAC;
            else
                ASSERT (0 && "unknown field type");
            read_one_layer (*i, lay, datatype);
            lay.vecfield = true;
        }
    }
}



bool
Field3DInput::open (const std::string &name, ImageSpec &newspec)
{
    oiio_field3d_initialize ();
    if (m_input)
        close();

    {
        lock_guard lock (field3d_pvt::field3d_mutex);
        m_input = new Field3DInputFile;
        if (! m_input->open (name)) {
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
    lock_guard lock (field3d_pvt::field3d_mutex);
    if (m_input) {
        m_input->close ();
        delete m_input;   // implicity closes
        m_input = NULL;
        m_name.clear ();
    }

    m_subimage = -1;
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
    return true;
}



bool
Field3DInput::read_native_tile (int x, int y, int z, void *data)
{
    lock_guard lock (field3d_pvt::field3d_mutex);
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


OIIO_PLUGIN_NAMESPACE_END

