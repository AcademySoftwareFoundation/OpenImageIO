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


#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include "ustring.h"
#include "imageio.h"



namespace OpenImageIO {

// Forward declaration
namespace pvt {

class ImageCacheImpl;
};





/// Define an API to an abstract class that that manages image files,
/// caches of open file handles as well as tiles of pixels so that truly
/// huge amounts of image data may be accessed by an application with low
/// memory footprint.
class ImageCache {
public:
    /// Create a ImageCache and return a pointer.  This should only be
    /// freed by passing it to ImageCache::destroy()!
    ///
    /// If shared==true, it's intended to be shared with other like-minded
    /// owners in the same process who also ask for a shared cache.  If
    /// false, a private image cache will be created.
    static ImageCache *create (bool shared=true);

    /// Destroy a ImageCache that was created using ImageCache::create().
    ///
    static void destroy (ImageCache * x);

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
    ///
    virtual bool attribute (const std::string &name, TypeDesc type, const void *val) = 0;
    // Shortcuts for common types
    virtual bool attribute (const std::string &name, int val) = 0;
    virtual bool attribute (const std::string &name, float val) = 0;
    virtual bool attribute (const std::string &name, double val) = 0;
    virtual bool attribute (const std::string &name, const char *val) = 0;
    virtual bool attribute (const std::string &name, const std::string &val) = 0;

    /// Get the named attribute, store it in value.
    virtual bool getattribute (const std::string &name, TypeDesc type, void *val) = 0;
    // Shortcuts for common types
    virtual bool getattribute (const std::string &name, int &val) = 0;
    virtual bool getattribute (const std::string &name, float &val) = 0;
    virtual bool getattribute (const std::string &name, double &val) = 0;
    virtual bool getattribute (const std::string &name, char **val) = 0;
    virtual bool getattribute (const std::string &name, std::string &val) = 0;


    /// Get information about the named imagee.  Return true if found
    /// and the data has been put in *data.  Return false if the image
    /// doesn't exist, doesn't have the requested data, if the data
    /// doesn't match the type requested. or some other failure.
    virtual bool get_image_info (ustring filename, ustring dataname,
                                 TypeDesc datatype, void *data) = 0;
    
    /// Get the ImageSpec associated with the named image (the first
    /// subimage, by default, or as set by 'subimage').  If the file is
    /// found and is an image format that can be read, store a copy of
    /// its specification in spec and return true.  Return false if the
    /// file was not found or could not be opened as an image file by
    /// any available ImageIO plugin.
    virtual bool get_imagespec (ustring filename, ImageSpec &spec,
                                int subimage=0) = 0;

    /// Retrieve the rectangle of pixels spanning
    /// [xmin..xmax X ymin..ymax X zmin..zmax] (inclusive, specified as
    /// integer pixel coordinates), at the named MIP-map level, storing
    /// the pixel values beginning at the address specified by result.
    /// The pixel values will be converted to the type specified by
    /// format.  It is up to the caller to ensure that result points to
    /// an area of memory big enough to accommodate the requested
    /// rectangle (taking into consideration its dimensions, number of
    /// channels, and data format).
    ///
    /// Return true if the file is found and could be opened by an
    /// available ImageIO plugin, otherwise return false.
    virtual bool get_pixels (ustring filename, int level,
                             int xmin, int xmax, int ymin, int ymax,
                             int zmin, int zmax, 
                             TypeDesc format, void *result) = 0;

    /// Define an opaque data type that allows us to have a pointer
    /// to a tile but without exposing any internals.
    class Tile;

    /// Find a tile given by an image filename, mipmap level, and pixel
    /// coordinates.  An opaque pointer to the tile will be returned,
    /// or NULL if no such file (or tile within the file) exists or can
    /// be read.  The tile will not be purged from the cache until 
    /// after release_tile() is called on the tile pointer.  This is
    /// thread-safe!
    virtual Tile * get_tile (ustring filename, int level,
                                int x, int y, int z) = 0;

    /// After finishing with a tile, release_tile will allow it to 
    /// once again be purged from the tile cache if required.
    virtual void release_tile (Tile *tile) const = 0;

    /// For a tile retrived by get_tile(), return a pointer to the
    /// pixel data itself, and also store in 'format' the data type that
    /// the pixels are internally stored in (which may be different than
    /// the data type of the pixels in the disk file).
    virtual const void * tile_pixels (Tile *tile, TypeDesc &format) const = 0;

    /// If any of the API routines returned false indicating an error,
    /// this routine will return the error string (and clear any error
    /// flags).  If no error has occurred since the last time geterror()
    /// was called, it will return an empty string.
    virtual std::string geterror () const = 0;

private:
    // Make delete private and unimplemented in order to prevent apps
    // from calling it.  Instead, they should call ImageCache::destroy().
    void operator delete (void *todel) { }
};


};  // end namespace OpenImageIO


#endif // IMAGECACHE_H
