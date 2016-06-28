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

#include <boost/regex.hpp>


#include "OpenImageIO/argparse.h"
#include "OpenImageIO/imageio.h"
#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/imagebufalgo.h"
#include "OpenImageIO/filesystem.h"
#include "OpenImageIO/filter.h"
#include "OpenImageIO/thread.h"

#include "oiiotool.h"

OIIO_NAMESPACE_USING
using namespace OiioTool;
using namespace ImageBufAlgo;



ImageRec::ImageRec (const std::string &name, int nsubimages,
                    const int *miplevels, const ImageSpec *specs)
    : m_name(name), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(true),
      m_was_output(false),
      m_imagecache(NULL)
{
    int specnum = 0;
    m_subimages.resize (nsubimages);
    for (int s = 0;  s < nsubimages;  ++s) {
        int nmips = miplevels ? miplevels[s] : 1;
        m_subimages[s].m_miplevels.resize (nmips);
        m_subimages[s].m_specs.resize (nmips);
        for (int m = 0;  m < nmips;  ++m) {
            ImageBuf *ib = specs ? new ImageBuf (specs[specnum])
                                 : new ImageBuf ();
            m_subimages[s].m_miplevels[m].reset (ib);
            if (specs)
                m_subimages[s].m_specs[m] = specs[specnum];
            ++specnum;
        }
    }
}



ImageRec::ImageRec (ImageRec &img, int subimage_to_copy,
                    int miplevel_to_copy, bool writable, bool copy_pixels)
    : m_name(img.name()), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(false),
      m_was_output(false),
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
                ib = new ImageBuf (srcspec);
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



ImageRec::ImageRec (ImageRec &A, ImageRec &B, int subimage_to_copy,
                    WinMerge pixwin, WinMerge fullwin,
                    TypeDesc pixeltype)
    : m_name(A.name()), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(false),
      m_was_output(false),
      m_imagecache(A.m_imagecache)
{
    A.read ();
    B.read ();
    int subimages = (subimage_to_copy < 0) ? 
                          std::min(A.subimages(), B.subimages()) : 1;
    int first_subimage = clamp (subimage_to_copy, 0, subimages-1);
    m_subimages.resize (subimages);
    for (int s = 0;  s < subimages;  ++s) {
        int srcsub = s + first_subimage;
        m_subimages[s].m_miplevels.resize (1);
        m_subimages[s].m_specs.resize (1);
        const ImageBuf &Aib (A(srcsub));
        const ImageBuf &Bib (B(srcsub));
        const ImageSpec &Aspec (Aib.spec());
        const ImageSpec &Bspec (Bib.spec());
        ImageSpec spec = Aspec;
        ROI Aroi = get_roi (Aspec), Aroi_full = get_roi_full (Aspec);
        ROI Broi = get_roi (Bspec), Broi_full = get_roi_full (Bspec);
        switch (pixwin) {
        case WinMergeUnion :
            set_roi (spec, roi_union (Aroi, Broi)); break;
        case WinMergeIntersection :
            set_roi (spec, roi_intersection (Aroi, Broi)); break;
        case WinMergeA :
            set_roi (spec, Aroi); break;
        case WinMergeB :
            set_roi (spec, Broi); break;
        }
        switch (fullwin) {
        case WinMergeUnion :
            set_roi_full (spec, roi_union (Aroi_full, Broi_full)); break;
        case WinMergeIntersection :
            set_roi_full (spec, roi_intersection (Aroi_full, Broi_full)); break;
        case WinMergeA :
            set_roi_full (spec, Aroi_full); break;
        case WinMergeB :
            set_roi_full (spec, Broi_full); break;
        }
        if (pixeltype != TypeDesc::UNKNOWN)
            spec.set_format (pixeltype);
        spec.nchannels = std::min (Aspec.nchannels, Bspec.nchannels);
        spec.channelnames.resize (spec.nchannels);
        spec.channelformats.clear ();

        ImageBuf *ib = new ImageBuf (spec);

        m_subimages[s].m_miplevels[0].reset (ib);
        m_subimages[s].m_specs[0] = spec;
    }
}



ImageRec::ImageRec (ImageBufRef img, bool copy_pixels)
    : m_name(img->name()), m_elaborated(true),
      m_metadata_modified(false), m_pixels_modified(false),
      m_was_output(false),
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
      m_was_output(false),
      m_imagecache(imagecache)
{
    int subimages = 1;
    m_subimages.resize (subimages);
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = 1;
        m_subimages[s].m_miplevels.resize (miplevels);
        m_subimages[s].m_specs.resize (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            ImageBuf *ib = new ImageBuf (spec);
            m_subimages[s].m_miplevels[m].reset (ib);
            m_subimages[s].m_specs[m] = spec;
        }
    }
}



bool
ImageRec::read (ReadPolicy readpolicy)
{
    if (elaborated())
        return true;
    static ustring u_subimages("subimages"), u_miplevels("miplevels");
    static boost::regex regex_sha ("SHA-1=[[:xdigit:]]*[ ]*");
    int subimages = 0;
    ustring uname (name());
    if (! m_imagecache->get_image_info (uname, 0, 0, u_subimages,
                                        TypeDesc::TypeInt, &subimages)) {
        error ("file not found: \"%s\"", name());
        return false;  // Image not found
    }
    m_subimages.resize (subimages);
    bool allok = true;
    for (int s = 0;  s < subimages;  ++s) {
        int miplevels = 0;
        m_imagecache->get_image_info (uname, s, 0, u_miplevels,
                                      TypeDesc::TypeInt, &miplevels);
        m_subimages[s].m_miplevels.resize (miplevels);
        m_subimages[s].m_specs.resize (miplevels);
        for (int m = 0;  m < miplevels;  ++m) {
            // Force a read now for reasonable-sized first images in the
            // file. This can greatly speed up the multithread case for
            // tiled images by not having multiple threads working on the
            // same image lock against each other on the file handle.
            // We guess that "reasonable size" is 50 MB, that's enough to
            // hold a 2048x1536 RGBA float image.  Larger things will 
            // simply fall back on ImageCache.
            bool forceread = (s == 0 && m == 0 &&
                              m_imagecache->imagespec(uname,s,m)->image_bytes() < 50*1024*1024);
            ImageBuf *ib = new ImageBuf (name(), m_imagecache);

            // If we were requested to bypass the cache, force a full read.
            if (readpolicy & ReadNoCache)
                forceread = true;

            // Convert to float unless asked to keep native.
            TypeDesc convert = (readpolicy & ReadNative)
                             ? ib->nativespec().format : TypeDesc::FLOAT;
            if (! forceread &&
                convert != TypeDesc::UINT8 && convert != TypeDesc::UINT16 &&
                convert != TypeDesc::HALF &&  convert != TypeDesc::FLOAT) {
                // If we're still trying to use the cache but it doesn't
                // support the native type, force a full read.
                forceread = true;
            }

            bool ok = ib->read (s, m, forceread, convert);
            if (!ok)
                error ("%s", ib->geterror());
            allok &= ok;
            // Remove any existing SHA-1 hash from the spec.
            ib->specmod().erase_attribute ("oiio:SHA-1");
            std::string desc = ib->spec().get_string_attribute ("ImageDescription");
            if (desc.size())
                ib->specmod().attribute ("ImageDescription",
                                         boost::regex_replace (desc, regex_sha, ""));

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
    return allok;
}


namespace {
static spin_mutex err_mutex;
}



bool
ImageRec::has_error () const
{
    spin_lock lock (err_mutex);
    return ! m_err.empty();
}



std::string
ImageRec::geterror (bool clear_error) const
{
    spin_lock lock (err_mutex);
    std::string e = m_err;
    if (clear_error)
        m_err.clear();
    return e;
}



void
ImageRec::append_error (string_view message) const
{
    spin_lock lock (err_mutex);
    ASSERT (m_err.size() < 1024*1024*16 &&
            "Accumulated error messages > 16MB. Try checking return codes!");
    if (m_err.size() && m_err[m_err.size()-1] != '\n')
        m_err += '\n';
    m_err += message;
}


