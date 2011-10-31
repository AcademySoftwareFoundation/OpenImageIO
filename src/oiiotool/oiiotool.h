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


#ifndef OIIOTOOL_H

#include "imagebuf.h"
#include "refcnt.h"


OIIO_NAMESPACE_ENTER {
namespace OiioTool {

typedef int (*CallbackFunction)(int argc,const char*argv[]);

class ImageRec;
typedef shared_ptr<ImageRec> ImageRecRef;



class Oiiotool {
public:
    // General options
    bool verbose;
    bool noclobber;
    bool allsubimages;
    bool printinfo;
    bool printstats;
    bool updatemode;
    int threads;
    
    // Output options
    TypeDesc output_dataformat;
    int output_bitspersample;
    bool output_scanline;
    int output_tilewidth, output_tileheight;
    std::string output_compression;
    int output_quality;
    std::string output_planarconfig;
    bool output_adjust_time;
    bool output_autocrop;

    // Options for --diff
    float diff_warnthresh;
    float diff_warnpercent;
    float diff_hardwarn;
    float diff_failthresh;
    float diff_failpercent;
    float diff_hardfail;

    // Internal state
    ImageRecRef curimg;                      // current image
    std::vector<ImageRecRef> image_stack;    // stack of previous images
    ImageCache *imagecache;                  // back ptr to ImageCache
    int return_value;                        // oiiotool command return code

    Oiiotool ()
        : verbose(false), noclobber(false), allsubimages(false),
          printinfo(false), printstats(false), updatemode(false),
          threads(0),
          output_dataformat(TypeDesc::UNKNOWN), output_bitspersample(0),
          output_scanline(false), output_tilewidth(0), output_tileheight(0),
          output_compression(""), output_quality(-1),
          output_planarconfig("default"),
          output_adjust_time(false), output_autocrop(true),
          diff_warnthresh(1.0e-6f), diff_warnpercent(0),
          diff_hardwarn(std::numeric_limits<float>::max()),
          diff_failthresh(1.0e-6f), diff_failpercent(0),
          diff_hardfail(std::numeric_limits<float>::max()),
          imagecache(NULL),
          return_value (EXIT_SUCCESS),
          m_pending_callback(NULL), m_pending_argc(0)
    {
    }

    // Force img to be read at this point.
    void read (ImageRecRef img);
    // Read the current image
    void read () { read (curimg); }

    // If required_images are not yet on the stack, then postpone this
    // call by putting it on the 'pending' list and return true.
    // Otherwise (if enough images are on the stack), return false.
    bool postpone_callback (int required_images, CallbackFunction func,
                            int argc, const char *argv[]);

    // Process any pending commands.
    void process_pending ();

    CallbackFunction pending_callback () const { return m_pending_callback; }
    const char *pending_callback_name () const { return m_pending_argv[0]; }

private:
    CallbackFunction m_pending_callback;
    int m_pending_argc;
    const char *m_pending_argv[4];
};


typedef shared_ptr<ImageBuf> ImageBufRef;


class SubimageRec {
public:
    int miplevels() const { return (int) m_miplevels.size(); }
    ImageBuf * operator() () {
        return miplevels() ? m_miplevels[0].get() : NULL;
    }
    ImageBuf * operator[] (int i) {
        return i < miplevels() ? m_miplevels[i].get() : NULL;
    }
    const ImageBuf * operator[] (int i) const {
        return i < miplevels() ? m_miplevels[i].get() : NULL;
    }
    ImageSpec * spec (int i) {
        return i < miplevels() ? &m_specs[i] : NULL;
    }
    const ImageSpec * spec (int i) const {
        return i < miplevels() ? &m_specs[i] : NULL;
    }
private:
    std::vector<ImageBufRef> m_miplevels;
    std::vector<ImageSpec> m_specs;
    friend class ImageRec;
};



class ImageRec {
public:
    ImageRec (const std::string &name, ImageCache *imagecache)
        : m_name(name), m_elaborated(false),
          m_metadata_modified(false), m_pixels_modified(false),
          m_imagecache(imagecache)
    { }

    // Copy an existing ImageRec.  Copy just the single subimage_to_copy
    // if >= 0, or all subimages if <0.  Copy mip levels only if
    // copy_miplevels is true, otherwise only the top MIP level.  If
    // writable is true, we expect to need to alter the pixels of the
    // resulting ImageRec.  If copy_pixels is false, just make the new
    // image big enough, no need to initialize the pixel values.
    ImageRec (ImageRec &img, int subimage_to_copy = -1,
              bool copy_miplevels = true, bool writable = true,
              bool copy_pixels = true);

    // Initialize an ImageRec with the given spec.
    ImageRec (const std::string &name, const ImageSpec &spec,
              ImageCache *imagecache);

    // Number of subimages
    int subimages() const { return (int) m_subimages.size(); }

    // Number of MIP levels of the given subimage
    int miplevels (int subimage=0) const {
        if (subimage >= subimages())
            return 0;
        return m_subimages[subimage].miplevels();
    }

    // Accessing it like an array returns a specific subimage
    SubimageRec& operator[] (int i) {
        return m_subimages[i];
    }
    const SubimageRec& operator[] (int i) const {
        return m_subimages[i];
    }

    std::string name () const { return m_name; }

    bool elaborated () const { return m_elaborated; }

    bool read ();

    // ir(subimg,mip) references a specific MIP level of a subimage
    // ir(subimg) references the first MIP level of a subimage
    // ir() references the first MIP level of the first subimage
    ImageBuf& operator() (int subimg=0, int mip=0) {
        return *m_subimages[subimg][mip];
    }
    const ImageBuf& operator() (int subimg=0, int mip=0) const {
        return *m_subimages[subimg][mip];
    }

    ImageSpec * spec (int subimg=0, int mip=0) {
        return subimg < subimages() ? m_subimages[subimg].spec(mip) : NULL;
    }
    const ImageSpec * spec (int subimg=0, int mip=0) const {
        return subimg < subimages() ? m_subimages[subimg].spec(mip) : NULL;
    }

    bool metadata_modified () const { return m_metadata_modified; }
    void metadata_modified (bool mod) { m_metadata_modified = mod; }
    bool pixels_modified () const { return m_pixels_modified; }
    void pixels_modified (bool mod) { m_pixels_modified = mod; }

    std::time_t time() const { return m_time; }

private:
    std::string m_name;
    bool m_elaborated;
    bool m_metadata_modified;
    bool m_pixels_modified;
    std::vector<SubimageRec> m_subimages;
    std::time_t m_time;  //< Modification time of the input file
    ImageCache *m_imagecache;
};




struct print_info_options {
    bool verbose;
    bool filenameprefix;
    bool sum;
    bool subimages;
    bool compute_sha1;
    bool compute_stats;
    std::string metamatch;
    size_t namefieldlength;

    print_info_options ()
        : verbose(false), filenameprefix(false), sum(false), subimages(false),
          compute_sha1(false), compute_stats(false), namefieldlength(20)
    {}
};


// Print info about the named file to stdout, using print_info_options
// opt for guidance on what to print and how to do it.  The total size
// of the uncompressed pixels in the file is returned in totalsize.  The
// return value will be true if everything is ok, or false if there is
// an error (in which case the error message will be stored in 'error').
bool print_info (const std::string &filename, 
                 const print_info_options &opt,
                 long long &totalsize, std::string &error);


// Modify the resolution and/or offset according to what's in geom.
// Valid geometries are WxH (resolution), +X+Y (offsets), WxH+X+Y
// (resolution and offset).  If 'allow_scaling' is true, geometries of
// S% (e.g. "50%") or just S (e.g., "1.2") will be accepted to scale the
// existing width and height (rounding to the nearest whole number of
// pixels.
bool adjust_geometry (int &w, int &h, int &x, int &y, const char *geom,
                      bool allow_scaling=false);

// Set an attribute of the given image.  The type should be one of
// TypeDesc::INT (decode the value as an int), FLOAT, STRING, or UNKNOWN
// (look at the string and try to discern whether it's an int, float, or
// string).  If the 'value' string is empty, it will delete the
// attribute.
bool set_attribute (ImageRecRef img, const std::string &attribname,
                    TypeDesc type, const std::string &value);

inline bool same_size (const ImageBuf &A, const ImageBuf &B)
{
    const ImageSpec &a (A.spec()), &b (B.spec());
    return (a.width == b.width && a.height == b.height &&
            a.depth == b.depth && a.nchannels == b.nchannels);
}


enum DiffErrors {
    DiffErrOK = 0,            ///< No errors, the images match exactly
    DiffErrWarn,              ///< Warning: the errors differ a little
    DiffErrFail,              ///< Failure: the errors differ a lot
    DiffErrDifferentSize,     ///< Images aren't even the same size
    DiffErrFile,              ///< Could not find or open input files, etc.
    DiffErrLast
};

int do_action_diff (ImageRec &ir0, ImageRec &ir1, Oiiotool &options);



// Helper template -- perform the action on each spec in the ImageRec.
// The action needs a signature like:
//     bool action(ImageSpec &spec, const T& t))
template<class Action, class Type>
bool apply_spec_mod (ImageRec &img, Action act, const Type &t,
                     bool allsubimages)
{
    bool ok = true;
    img.read ();
    img.metadata_modified (true);
    for (int s = 0, send = img.subimages();  s < send;  ++s) {
        for (int m = 0, mend = img.miplevels(s);  m < mend;  ++m) {
            ok &= act (*img.spec(s,m), t);
            if (! allsubimages)
                break;
        }
        if (! allsubimages)
            break;
    }
    return ok;
}


} // OiioTool namespace
} OIIO_NAMESPACE_EXIT;


#endif // OIIOTOOL_H
