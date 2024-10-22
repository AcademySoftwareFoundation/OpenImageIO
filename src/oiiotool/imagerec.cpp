// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "oiiotool.h"

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/filter.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagecache.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/thread.h>

using namespace OIIO;
using namespace OiioTool;
using namespace ImageBufAlgo;



ImageRec::ImageRec(const std::string& name, int nsubimages,
                   cspan<int> miplevels, cspan<ImageSpec> specs)
    : m_name(name)
    , m_elaborated(true)
{
    int specnum = 0;
    m_subimages.resize(nsubimages);
    for (int s = 0; s < nsubimages; ++s) {
        int nmips = miplevels.size() ? miplevels[s] : 1;
        m_subimages[s].m_miplevels.resize(nmips);
        m_subimages[s].m_specs.resize(nmips);
        for (int m = 0; m < nmips; ++m) {
            ImageBuf* ib = specs.size() ? new ImageBuf(specs[specnum])
                                        : new ImageBuf();
            m_subimages[s].m_miplevels[m].reset(ib);
            if (specs.size())
                m_subimages[s].m_specs[m] = specs[specnum];
            ++specnum;
        }
    }
}



ImageRec::ImageRec(ImageRec& img, int subimage_to_copy, int miplevel_to_copy,
                   bool writable, bool copy_pixels)
    : m_name(img.name())
    , m_elaborated(true)
    , m_imagecache(img.m_imagecache)
{
    img.read();
    if (subimage_to_copy >= img.subimages()) {
        errorfmt("Selecting subimage {}, but there are only {} subimages",
                 subimage_to_copy, img.subimages());
        subimage_to_copy = img.subimages() - 1;
    }
    int first_subimage = clamp(subimage_to_copy, 0, img.subimages() - 1);
    int subimages      = (subimage_to_copy < 0) ? img.subimages() : 1;
    m_subimages.resize(subimages);
    for (int s = 0; s < subimages; ++s) {
        int srcsub = s + first_subimage;
        if (miplevel_to_copy >= img.miplevels(srcsub)) {
            errorfmt(
                "Selecting MIP level {} of subimage {}, which has only {} MIP levels",
                miplevel_to_copy, srcsub, img.miplevels(srcsub));
            miplevel_to_copy = img.miplevels(srcsub) - 1;
        }
        int first_miplevel = clamp(miplevel_to_copy, 0,
                                   img.miplevels(srcsub) - 1);
        int miplevels      = (miplevel_to_copy < 0) ? img.miplevels(srcsub) : 1;
        m_subimages[s].m_miplevels.resize(miplevels);
        m_subimages[s].m_specs.resize(miplevels);
        for (int m = 0; m < miplevels; ++m) {
            int srcmip = m + first_miplevel;
            const ImageBuf& srcib(img(srcsub, srcmip));
            const ImageSpec& srcspec(*img.spec(srcsub, srcmip));
            ImageBuf* ib = NULL;
            if (writable || img.pixels_modified() || !copy_pixels) {
                // Make our own copy of the pixels
                ib = new ImageBuf(srcspec);
                if (copy_pixels)
                    ib->copy_pixels(srcib);
            } else {
                // The other image is not modified, and we don't need to be
                // writable, either.
                ib      = new ImageBuf(img.name(), 0, 0, srcib.imagecache());
                bool ok = ib->read(srcsub, srcmip, false /*force*/,
                                   img.m_input_dataformat /*convert*/);
                OIIO_ASSERT(ok);
            }
            m_subimages[s].m_miplevels[m].reset(ib);
            m_subimages[s].m_specs[m] = srcspec;
        }
    }
}



ImageRec::ImageRec(ImageRec& A, ImageRec& B, int subimage_to_copy,
                   WinMerge pixwin, WinMerge fullwin, TypeDesc pixeltype)
    : m_name(A.name())
    , m_elaborated(true)
    , m_imagecache(A.m_imagecache)
{
    A.read();
    B.read();
    int subimages      = (subimage_to_copy < 0)
                             ? std::min(A.subimages(), B.subimages())
                             : 1;
    int first_subimage = clamp(subimage_to_copy, 0, subimages - 1);
    m_subimages.resize(subimages);
    for (int s = 0; s < subimages; ++s) {
        int srcsub = s + first_subimage;
        m_subimages[s].m_miplevels.resize(1);
        m_subimages[s].m_specs.resize(1);
        const ImageBuf& Aib(A(srcsub));
        const ImageBuf& Bib(B(srcsub));
        const ImageSpec& Aspec(Aib.spec());
        const ImageSpec& Bspec(Bib.spec());
        ImageSpec spec = Aspec;
        ROI Aroi = get_roi(Aspec), Aroi_full = get_roi_full(Aspec);
        ROI Broi = get_roi(Bspec), Broi_full = get_roi_full(Bspec);
        switch (pixwin) {
        case WinMergeUnion: set_roi(spec, roi_union(Aroi, Broi)); break;
        case WinMergeIntersection:
            set_roi(spec, roi_intersection(Aroi, Broi));
            break;
        case WinMergeA: set_roi(spec, Aroi); break;
        case WinMergeB: set_roi(spec, Broi); break;
        }
        switch (fullwin) {
        case WinMergeUnion:
            set_roi_full(spec, roi_union(Aroi_full, Broi_full));
            break;
        case WinMergeIntersection:
            set_roi_full(spec, roi_intersection(Aroi_full, Broi_full));
            break;
        case WinMergeA: set_roi_full(spec, Aroi_full); break;
        case WinMergeB: set_roi_full(spec, Broi_full); break;
        }
        if (pixeltype != TypeDesc::UNKNOWN)
            spec.set_format(pixeltype);
        spec.nchannels = std::min(Aspec.nchannels, Bspec.nchannels);
        spec.channelnames.resize(spec.nchannels);
        spec.channelformats.clear();

        ImageBuf* ib = new ImageBuf(spec);

        m_subimages[s].m_miplevels[0].reset(ib);
        m_subimages[s].m_specs[0] = spec;
    }
}



ImageRec::ImageRec(ImageBufRef img, bool copy_pixels)
    : m_name(img->name())
    , m_elaborated(true)
    , m_imagecache(img->imagecache())
{
    m_subimages.resize(1);
    m_subimages[0].m_miplevels.resize(1);
    m_subimages[0].m_specs.push_back(img->spec());
    if (copy_pixels) {
        m_subimages[0].m_miplevels[0].reset(new ImageBuf(*img));
    } else {
        m_subimages[0].m_miplevels[0] = img;
    }
}



ImageRec::ImageRec(const std::string& name, const ImageSpec& spec,
                   std::shared_ptr<ImageCache> imagecache)
    : m_name(name)
    , m_elaborated(true)
    , m_pixels_modified(true)
    , m_imagecache(imagecache)
{
    int subimages = 1;
    m_subimages.resize(subimages);
    for (int s = 0; s < subimages; ++s) {
        int miplevels = 1;
        m_subimages[s].m_miplevels.resize(miplevels);
        m_subimages[s].m_specs.resize(miplevels);
        for (int m = 0; m < miplevels; ++m) {
            ImageBuf* ib = new ImageBuf(spec);
            m_subimages[s].m_miplevels[m].reset(ib);
            m_subimages[s].m_specs[m] = spec;
        }
    }
}



bool
ImageRec::read_nativespec()
{
    if (elaborated())
        return true;
    // If m_subimages has already been resized, we've been here before.
    if (m_subimages.size())
        return true;

    static ustring u_subimages("subimages"), u_miplevels("miplevels");
    int subimages = 0;
    ustring uname(name());
    if (!m_imagecache->get_image_info(uname, 0, 0, u_subimages, TypeInt,
                                      &subimages)) {
        errorfmt("file not found: \"{}\"", name());
        return false;  // Image not found
    }
    m_subimages.resize(subimages);
    bool allok = true;
    for (int s = 0; s < subimages; ++s) {
        int miplevels = 0;
        m_imagecache->get_image_info(uname, s, 0, u_miplevels, TypeInt,
                                     &miplevels);
        m_subimages[s].m_miplevels.resize(miplevels);
        m_subimages[s].m_specs.resize(miplevels);
        m_subimages[s].m_was_direct_read = true;
        for (int m = 0; m < miplevels; ++m) {
            ImageBufRef ib(
                new ImageBuf(name(), s, m, m_imagecache, m_configspec.get()));
            bool ok = ib->init_spec(name(), s, m);
            if (!ok)
                errorfmt("{}", ib->geterror());
            allok &= ok;
            m_subimages[s].m_miplevels[m] = ib;
            m_subimages[s].m_specs[m]     = ib->spec();
        }
    }

    return allok;
}



bool
ImageRec::read(ReadPolicy readpolicy, string_view channel_set)
{
    if (elaborated())
        return true;
    static ustring u_subimages("subimages"), u_miplevels("miplevels");
    int subimages = 0;
    ustring uname(name());
    if (!m_imagecache->get_image_info(uname, 0, 0, u_subimages, TypeInt,
                                      &subimages)) {
        errorfmt("file not found: \"{}\"", name());
        return false;  // Image not found
    }
    m_subimages.resize(subimages);
    bool allok = true;
    ErrorHandler eh;
    for (int s = 0; s < subimages; ++s) {
        int miplevels = 0;
        m_imagecache->get_image_info(uname, s, 0, u_miplevels, TypeInt,
                                     &miplevels);
        m_subimages[s].m_miplevels.resize(miplevels);
        m_subimages[s].m_specs.resize(miplevels);
        m_subimages[s].m_was_direct_read = true;
        for (int m = 0; m < miplevels; ++m) {
            // Force a read now for reasonable-sized images in the
            // file. This can greatly speed up the multithread case for
            // tiled images by not having multiple threads working on the
            // same image lock against each other on the file handle.
            // We guess that "reasonable size" is 50 MB, that's enough to
            // hold a 2048x1536 RGBA float image.  Larger things will
            // simply fall back on ImageCache. By multiplying by the number
            // of subimages (a.k.a. frames in a movie), we also push movies
            // relying on the cache to read their frames on demand rather
            // than reading the whole movie up front, even though each frame
            // individually would be well below the threshold.
            ImageSpec spec;
            m_imagecache->get_imagespec(uname, spec, s);
            m_imagecache->get_cache_dimensions(uname, spec, s, m);
            imagesize_t imgbytes = spec.image_bytes();
            bool forceread       = (s == 0 && m == 0
                              && imgbytes * subimages < 50 * 1024 * 1024);
            ImageBufRef ib(
                new ImageBuf(name(), s, m, m_imagecache, configspec()));

            bool post_channel_set_action = false;
            std::vector<std::string> newchannelnames;
            std::vector<int> channel_set_channels;
            std::vector<float> channel_set_values;
            int new_alpha_channel = -1;
            int new_z_channel     = -1;
            int chbegin = 0, chend = -1;
            if (channel_set.size()) {
                decode_channel_set(ib->nativespec(), channel_set,
                                   newchannelnames, channel_set_channels,
                                   channel_set_values, eh);
                for (size_t c = 0, e = channel_set_channels.size(); c < e;
                     ++c) {
                    if (channel_set_channels[c] < 0)
                        post_channel_set_action = true;  // value fill-in
                    else if (c >= 1
                             && channel_set_channels[c]
                                    != channel_set_channels[c - 1] + 1)
                        post_channel_set_action = true;  // non-consecutive chans
                    if (channel_set_channels[c] == ib->spec().alpha_channel)
                        new_alpha_channel = c;
                    if (channel_set_channels[c] == ib->spec().z_channel)
                        new_z_channel = c;
                }
                if (ib->deep())
                    post_channel_set_action = true;
                if (!post_channel_set_action) {
                    chbegin   = channel_set_channels.front();
                    chend     = channel_set_channels.back() + 1;
                    forceread = true;
                }
            }

            // If we were requested to bypass the cache, force a full read.
            if (readpolicy & ReadNoCache)
                forceread = true;

            // Convert to float unless asked to keep native or override.
            TypeDesc convert = TypeDesc::FLOAT;
            if (m_input_dataformat != TypeDesc::UNKNOWN) {
                convert = m_input_dataformat;
                if (m_input_dataformat != ib->nativespec().format)
                    m_subimages[s].m_was_direct_read = false;
                forceread = true;
            } else if (readpolicy & ReadNative)
                convert = ib->nativespec().format;
            if (!forceread && convert != TypeDesc::UINT8
                && convert != TypeDesc::UINT16 && convert != TypeDesc::HALF
                && convert != TypeDesc::FLOAT) {
                // If we're still trying to use the cache but it doesn't
                // support the native type, force a full read.
                forceread = true;
            }

            bool ok = ib->read(s, m, chbegin, chend, forceread, convert);
            if (ok && post_channel_set_action) {
                ImageBufRef allchan_buf(new ImageBuf);
                std::swap(allchan_buf, ib);
                ok = ImageBufAlgo::channels(*ib, *allchan_buf,
                                            (int)channel_set_channels.size(),
                                            channel_set_channels,
                                            channel_set_values, newchannelnames,
                                            false);
            }
            if (!ok)
                errorfmt("{}", ib->geterror());
            if (channel_set.size()) {
                // Adjust the spec to reflect the new channel set
                ib->specmod().alpha_channel = new_alpha_channel;
                ib->specmod().z_channel     = new_z_channel;
            }

            allok &= ok;
            // Remove any existing SHA-1 hash from the spec.
            ib->specmod().erase_attribute("oiio:SHA-1");
            std::string desc = ib->spec().get_string_attribute(
                "ImageDescription");
            if (desc.size()) {
                Strutil::excise_string_after_head(desc, "oiio:SHA-1=");
                ib->specmod().attribute("ImageDescription", desc);
            }

            m_subimages[s].m_miplevels[m] = ib;
            m_subimages[s].m_specs[m]     = ib->spec();
            // For ImageRec purposes, we need to restore a few of the
            // native settings.
            const ImageSpec& nativespec(ib->nativespec());
            // m_subimages[s].m_specs[m].format = nativespec.format;
            m_subimages[s].m_specs[m].tile_width  = nativespec.tile_width;
            m_subimages[s].m_specs[m].tile_height = nativespec.tile_height;
            m_subimages[s].m_specs[m].tile_depth  = nativespec.tile_depth;
        }
    }

    m_time       = Filesystem::last_write_time(name());
    m_elaborated = true;
    return allok;
}


namespace {
static spin_mutex err_mutex;
}



bool
ImageRec::has_error() const
{
    spin_lock lock(err_mutex);
    return !m_err.empty();
}



std::string
ImageRec::geterror(bool clear_error) const
{
    spin_lock lock(err_mutex);
    std::string e = m_err;
    if (clear_error)
        m_err.clear();
    return e;
}



void
ImageRec::append_error(string_view message) const
{
    spin_lock lock(err_mutex);
    OIIO_ASSERT(
        m_err.size() < 1024 * 1024 * 16
        && "Accumulated error messages > 16MB. Try checking return codes!");
    if (m_err.size() && m_err[m_err.size() - 1] != '\n')
        m_err += '\n';
    m_err += std::string(message);
}
