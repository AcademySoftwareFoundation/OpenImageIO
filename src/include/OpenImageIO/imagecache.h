/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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


/// \file
/// An API for accessing images via a system that
/// automatically manages a cache of resident image data.


#ifndef OPENIMAGEIO_IMAGECACHE_H
#define OPENIMAGEIO_IMAGECACHE_H

#include "OpenImageIO/ustring.h"
#include "OpenImageIO/imageio.h"


OIIO_NAMESPACE_ENTER
{

namespace pvt {
// Forward declaration
class ImageCacheImpl;
};




/// Define an API to an abstract class that manages image files,
/// caches of open file handles as well as tiles of pixels so that truly
/// huge amounts of image data may be accessed by an application with low
/// memory footprint.
class OIIO_API ImageCache {
public:
    /// Create a ImageCache and return a pointer.  This should only be
    /// freed by passing it to ImageCache::destroy()!
    ///
    /// If shared==true, it's intended to be shared with other like-minded
    /// owners in the same process who also ask for a shared cache.  If
    /// false, a private image cache will be created.
    static ImageCache *create (bool shared=true);

    /// Destroy a ImageCache that was created using ImageCache::create().
    /// The variety that takes a 'teardown' parameter, when set to true,
    /// will fully destroy even a "shared" ImageCache.
    static void destroy (ImageCache * x);
    static void destroy (ImageCache * x, bool teardown);

    ImageCache (void) { }
    virtual ~ImageCache () { }

    /// Close everything, free resources, start from scratch.
    ///
    virtual void clear () = 0;

    /// Set an attribute controlling the image cache.  Return true
    /// if the name and type were recognized and the attrib was set.
    /// Documented attributes:
    ///     int max_open_files : maximum number of file handles held open
    ///     float max_memory_MB : maximum tile cache size, in MB
    ///     string searchpath : colon-separated search path for images
    ///     string plugin_searchpath : colon-separated search path for plugins
    ///     int autotile : if >0, tile size to emulate for non-tiled images
    ///     int autoscanline : autotile using full width tiles
    ///     int automip : if nonzero, emulate mipmap on the fly
    ///     int accept_untiled : if nonzero, accept untiled images, but
    ///                          if zero, reject untiled images (default=1)
    ///     int accept_unmipped : if nonzero, accept unmipped images (def=1)
    ///     int statistics:level : verbosity of statistics auto-printed.
    ///     int forcefloat : if nonzero, convert all to float.
    ///     int failure_retries : number of times to retry a read before fail.
    ///     int deduplicate : if nonzero, detect duplicate textures (default=1)
    ///     string substitute_image : uses the named image in place of all
    ///                               texture and image references.
    ///     int unassociatedalpha : if nonzero, keep unassociated alpha images
    ///
    virtual bool attribute (string_view name, TypeDesc type,
                            const void *val) = 0;
    // Shortcuts for common types
    virtual bool attribute (string_view name, int val) = 0;
    virtual bool attribute (string_view name, float val) = 0;
    virtual bool attribute (string_view name, double val) = 0;
    virtual bool attribute (string_view name, string_view val) = 0;

    /// Get the named attribute, store it in value.
    virtual bool getattribute (string_view name, TypeDesc type,
                               void *val) = 0;
    // Shortcuts for common types
    virtual bool getattribute (string_view name, int &val) = 0;
    virtual bool getattribute (string_view name, float &val) = 0;
    virtual bool getattribute (string_view name, double &val) = 0;
    virtual bool getattribute (string_view name, char **val) = 0;
    virtual bool getattribute (string_view name, std::string &val) = 0;

    /// Given possibly-relative 'filename', resolve it using the search
    /// path rules and return the full resolved filename.
    virtual std::string resolve_filename (const std::string &filename) const=0;

    /// Get information about the named image.  Return true if found
    /// and the data has been put in *data.  Return false if the image
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool get_image_info (ustring filename, int subimage, int miplevel,
                         ustring dataname, TypeDesc datatype, void *data) = 0;

    /// Back-compatible version of get_image_info -- DEPRECATED
    bool get_image_info (ustring filename, ustring dataname,
                         TypeDesc datatype, void *data) {
        return get_image_info (filename, 0, 0, dataname, datatype, data);
    }

    /// Get the ImageSpec associated with the named image (the first
    /// subimage & miplevel by default, or as set by 'subimage' and
    /// 'miplevel').  If the file is found and is an image format that
    /// can be read, store a copy of its specification in spec and
    /// return true.  Return false if the file was not found or could
    /// not be opened as an image file by any available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, ImageSpec &spec,
                                int subimage=0, int miplevel=0,
                                bool native=false) = 0;

    /// Return a pointer to an ImageSpec associated with the named image
    /// (the first subimage & miplevel by default, or as set by
    /// 'subimage' and 'miplevel') if the file is found and is an image
    /// format that can be read, otherwise return NULL.
    ///
    /// This method is much more efficient than get_imagespec(), since
    /// it just returns a pointer to the spec held internally by the
    /// ImageCache (rather than copying the spec to the user's memory).
    /// However, the caller must beware that the pointer is only valid
    /// as long as nobody (even other threads) calls invalidate() on the
    /// file, or invalidate_all(), or destroys the ImageCache.
    virtual const ImageSpec *imagespec (ustring filename, int subimage=0,
                                        int miplevel=0, bool native=false) = 0;

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) X [zbegin..zend), with "exclusive end" a la STL,
    /// specified as integer pixel coordinates in the designated
    /// subimage & miplevel, storing the pixel values beginning at the
    /// address specified by result.  The pixel values will be converted
    /// to the type specified by format.  It is up to the caller to
    /// ensure that result points to an area of memory big enough to
    /// accommodate the requested rectangle (taking into consideration
    /// its dimensions, number of channels, and data format).  Requested
    /// pixels outside the valid pixel data region will be filled in
    /// with 0 values.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool get_pixels (ustring filename, int subimage, int miplevel,
                             int xbegin, int xend, int ybegin, int yend,
                             int zbegin, int zend,
                             TypeDesc format, void *result) = 0;

    /// Retrieve the rectangle of pixels spanning [xbegin..xend) X
    /// [ybegin..yend) X [zbegin..zend), channels [chbegin..chend), 
    /// with "exclusive end" a la STL, specified as integer pixel
    /// coordinates in the designated subimage & miplevel, storing the
    /// pixel values beginning at the address specified by result and
    /// with the given x, y, and z strides (in bytes). The pixel values
    /// will be converted to the type specified by format.  If the
    /// strides are set to AutoStride, they will be automatically
    /// computed assuming a contiguous data layout.  It is up to the
    /// caller to ensure that result points to an area of memory big
    /// enough to accommodate the requested rectangle (taking into
    /// consideration its dimensions, number of channels, and data
    /// format).  Requested pixels outside the valid pixel data region
    /// will be filled in with 0 values.
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool get_pixels (ustring filename,
                    int subimage, int miplevel, int xbegin, int xend,
                    int ybegin, int yend, int zbegin, int zend,
                    int chbegin, int chend, TypeDesc format, void *result,
                    stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                    stride_t zstride=AutoStride) = 0;

    /// Define an opaque data type that allows us to have a pointer
    /// to a tile but without exposing any internals.
    class Tile;

    /// Find a tile given by an image filename, subimage & miplevel, and
    /// pixel coordinates.  An opaque pointer to the tile will be
    /// returned, or NULL if no such file (or tile within the file)
    /// exists or can be read.  The tile will not be purged from the
    /// cache until after release_tile() is called on the tile pointer
    /// the same number of times that get_tile() was called (refcnt).
    /// This is thread-safe!
    virtual Tile * get_tile (ustring filename, int subimage, int miplevel,
                                int x, int y, int z) = 0;

    /// After finishing with a tile, release_tile will allow it to
    /// once again be purged from the tile cache if required.
    virtual void release_tile (Tile *tile) const = 0;

    /// For a tile retrived by get_tile(), return a pointer to the
    /// pixel data itself, and also store in 'format' the data type that
    /// the pixels are internally stored in (which may be different than
    /// the data type of the pixels in the disk file).
    virtual const void * tile_pixels (Tile *tile, TypeDesc &format) const = 0;

    /// This creates a file entry in the cache that, instead of reading
    /// from disk, uses a custom ImageInput to generate the image (note
    /// that it will have no effect if there's already an image by the
    /// same name in the cache).  The 'creator' is a factory that
    /// creates the custom ImageInput and will be called like this:
    /// ImageInput *in = creator(); Once created, the ImageCache owns
    /// the ImageInput and is responsible for destroying it when done.
    /// Custom ImageInputs allow "procedural" images, among other
    /// things.  Also, this is the method you use to set up a
    /// "writeable" ImageCache images (perhaps with a type of ImageInput
    /// that's just a stub that does as little as possible).
    virtual bool add_file (ustring filename, ImageInput::Creator creator) = 0;

    /// Preemptively add a tile corresponding to the named image, at the
    /// given subimage and MIP level.  The tile added is the one whose
    /// corner is (x,y,z), and buffer points to the pixels (in the given
    /// format, with supplied strides) which will be copied and inserted
    /// into the cache and made available for future lookups.
    virtual bool add_tile (ustring filename, int subimage, int miplevel,
                     int x, int y, int z, TypeDesc format, const void *buffer,
                     stride_t xstride=AutoStride, stride_t ystride=AutoStride,
                     stride_t zstride=AutoStride) = 0;

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    virtual std::string geterror () const = 0;

    /// Return the statistics output as a huge string.
    ///
    virtual std::string getstats (int level=1) const = 0;

    /// Reset most statistics to be as they were with a fresh
    /// ImageCache.  Caveat emptor: this does not flush the cache itelf,
    /// so the resulting statistics from the next set of texture
    /// requests will not match the number of tile reads, etc., that
    /// would have resulted from a new ImageCache.
    virtual void reset_stats () = 0;

    /// Invalidate any loaded tiles or open file handles associated with
    /// the filename, so that any subsequent queries will be forced to
    /// re-open the file or re-load any tiles (even those that were
    /// previously loaded and would ordinarily be reused).  A client
    /// might do this if, for example, they are aware that an image
    /// being held in the cache has been updated on disk.  This is safe
    /// to do even if other procedures are currently holding
    /// reference-counted tile pointers from the named image, but those
    /// procedures will not get updated pixels until they release the
    /// tiles they are holding.
    virtual void invalidate (ustring filename) = 0;

    /// Invalidate all loaded tiles and open file handles.  This is safe
    /// to do even if other procedures are currently holding
    /// reference-counted tile pointers from the named image, but those
    /// procedures will not get updated pixels until they release the
    /// tiles they are holding.  If force is true, everything will be
    /// invalidated, no matter how wasteful it is, but if force is
    /// false, in actuality files will only be invalidated if their
    /// modification times have been changed since they were first
    /// opened.
    virtual void invalidate_all (bool force=false) = 0;

private:
    // Make delete private and unimplemented in order to prevent apps
    // from calling it.  Instead, they should call ImageCache::destroy().
    void operator delete (void * /*todel*/) { }
};


}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IMAGECACHE_H
