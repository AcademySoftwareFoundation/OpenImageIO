/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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
#include <iostream>
#include <iterator>
#include <vector>
#include <string>
#include <utility>

#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

using boost::algorithm::iequals;


#include "argparse.h"
#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "filesystem.h"
#include "filter.h"

#include "oiiotool.h"

OIIO_NAMESPACE_USING
using namespace OiioTool;
using namespace ImageBufAlgo;



ImageRec::ImageRec (ImageRec &img, int subimage_to_copy,
                    int miplevel_to_copy, bool writable, bool copy_pixels)
    : m_name(img.name()), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(false),
      m_imagecache(img.m_imagecache)
{
    img.read ();
    int first_subimage = std::max (0, subimage_to_copy);
    int subimages = (subimage_to_copy < 0) ? img.subimages() : 1;
    m_subimages.resize (subimages);
    for (int s = 0;  s < subimages;  ++s) {
        int srcsub = s + first_subimage;
        int first_miplevel = std::max (0, miplevel_to_copy);
        int miplevels = (miplevel_to_copy < 0) ? img.miplevels(srcsub) : 1;
        m_subimages[s].m_miplevels.resize (miplevels);
        m_subimages[s].m_specs.resize (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            int srcmip = m + first_miplevel;
            const ImageBuf &srcib (img(srcsub,srcmip));
            const ImageSpec &srcspec (*img.spec(srcsub,srcmip));
            ImageBuf *ib = NULL;
            if (writable || img.pixels_modified() || !copy_pixels) {
                // Make our own copy of the pixels
                ib = new ImageBuf (img.name(), srcspec);
                if (copy_pixels)
                    ib->copy_pixels (srcib);
            } else {
                // The other image is not modified, and we don't need to be
                // writable, either.
                ib = new ImageBuf (img.name(), srcib.imagecache());
                bool ok = ib->read (srcsub, srcmip);
                ASSERT (ok);
            }
            m_subimages[s].m_miplevels[m].reset (ib);
            m_subimages[s].m_specs[m] = srcspec;
        }
    }
}



ImageRec::ImageRec (ImageBufRef img, bool copy_pixels)
    : m_name(img->name()), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(false),
      m_imagecache(img->imagecache())
{
    m_subimages.resize (1);
    m_subimages[0].m_miplevels.resize (1);
    m_subimages[0].m_specs.push_back (img->spec());
    if (copy_pixels) {
        m_subimages[0].m_miplevels[0].reset (new ImageBuf (*img));
    } else {
        m_subimages[0].m_miplevels[0] = img;
    }
}



ImageRec::ImageRec (const std::string &name, const ImageSpec &spec,
                    ImageCache *imagecache)
    : m_name(name), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(true),
      m_imagecache(imagecache)
{
    int subimages = 1;
    m_subimages.resize (subimages);
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = 1;
        m_subimages[s].m_miplevels.resize (miplevels);
        m_subimages[s].m_specs.resize (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageBuf *ib = new ImageBuf (name, spec);
            m_subimages[s].m_miplevels[m].reset (ib);
            m_subimages[s].m_specs[m] = spec;
        }
    }
}



ImageRec::ImageRec (const std::string &name, int nsubimages,
                    const int *miplevels, const ImageSpec *specs)
    : m_name(name), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(true),
      m_imagecache(NULL)
{
    int specnum = 0;
    m_subimages.resize (nsubimages);
    for (int s = 0;  s < nsubimages;  ++s) {
        m_subimages[s].m_miplevels.resize (miplevels[s]);
        m_subimages[s].m_specs.resize (miplevels[s]);
        for (int m = 0;  m < miplevels[s];  ++m) {
            ImageBuf *ib = new ImageBuf (name, specs[specnum]);
            m_subimages[s].m_miplevels[m].reset (ib);
            m_subimages[s].m_specs[m] = specs[specnum];
            ++specnum;
        }
    }
}



bool
ImageRec::read ()
{
    if (elaborated())
        return true;
    static ustring u_subimages("subimages"), u_miplevels("miplevels");
    int subimages = 0;
    if (! m_imagecache->get_image_info (ustring(name()), 0, 0, u_subimages,
                                        TypeDesc::TypeInt, &subimages)) {
        std::cerr << "ERROR: file \"" << name() << "\" not found.\n";
        return false;  // Image not found
    }
    m_subimages.resize (subimages);

    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = 0;
        m_imagecache->get_image_info (ustring(name()), s, 0, u_miplevels,
                                      TypeDesc::TypeInt, &miplevels);
        m_subimages[s].m_miplevels.resize (miplevels);
        m_subimages[s].m_specs.resize (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageBuf *ib = new ImageBuf (name(), m_imagecache);
            bool ok = ib->read (s, m);
            ASSERT (ok);
            m_subimages[s].m_miplevels[m].reset (ib);
            m_subimages[s].m_specs[m] = ib->spec();
            // For ImageRec purposes, we need to restore a few of the
            // native settings.
            const ImageSpec &nativespec (ib->nativespec());
            // m_subimages[s].m_specs[m].format = nativespec.format;
            m_subimages[s].m_specs[m].tile_width  = nativespec.tile_width;
            m_subimages[s].m_specs[m].tile_height = nativespec.tile_height;
            m_subimages[s].m_specs[m].tile_depth  = nativespec.tile_depth;
        }
    }

    m_time = Filesystem::last_write_time (name());
    m_elaborated = true;
    return true;
}
