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

#include "OpenImageIO/imagebuf.h"
#include "OpenImageIO/refcnt.h"
#include "OpenImageIO/timer.h"


OIIO_NAMESPACE_ENTER {
namespace OiioTool {

typedef int (*CallbackFunction)(int argc,const char*argv[]);

class ImageRec;
typedef shared_ptr<ImageRec> ImageRecRef;



class Oiiotool {
public:
    // General options
    bool verbose;
    bool runstats;
    bool noclobber;
    bool allsubimages;
    bool printinfo;
    bool printstats;
    bool dumpdata;
    bool dumpdata_showempty;
    bool hash;
    bool updatemode;
    int threads;
    std::string full_command_line;
    std::string printinfo_metamatch;
    std::string printinfo_nometamatch;

    // Output options
    TypeDesc output_dataformat;
    std::map<std::string,std::string> output_channelformats;
    int output_bitspersample;
    bool output_scanline;
    int output_tilewidth, output_tileheight;
    std::string output_compression;
    int output_quality;
    std::string output_planarconfig;
    bool output_adjust_time;
    bool output_autocrop;
    bool output_autotrim;
    bool output_dither;
    bool output_force_tiles; // for debugging

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
    std::map<std::string, ImageRecRef> image_labels; // labeled images
    ImageCache *imagecache;                  // back ptr to ImageCache
    int return_value;                        // oiiotool command return code
    ColorConfig colorconfig;                 // OCIO color config
    Timer total_readtime;
    Timer total_writetime;
    double total_imagecache_readtime;
    typedef std::map<std::string, double> TimingMap;
    TimingMap function_times;
    bool enable_function_timing;

    Oiiotool ();

    void clear_options ();

    // Force img to be read at this point.
    bool read (ImageRecRef img);
    // Read the current image
    bool read () {
        if (curimg)
            return read (curimg);
        return true;
    }

    // If required_images are not yet on the stack, then postpone this
    // call by putting it on the 'pending' list and return true.
    // Otherwise (if enough images are on the stack), return false.
    bool postpone_callback (int required_images, CallbackFunction func,
                            int argc, const char *argv[]);

    // Process any pending commands.
    void process_pending ();

    CallbackFunction pending_callback () const { return m_pending_callback; }
    const char *pending_callback_name () const { return m_pending_argv[0]; }

    void push (const ImageRecRef &img) {
        if (img) {
            if (curimg)
                image_stack.push_back (curimg);
            curimg = img;
        }
    }

    void push (ImageRec *newir) { push (ImageRecRef(newir)); }

    ImageRecRef pop () {
        ImageRecRef r = curimg;
        if (image_stack.size()) {
            // There are images on the full stack -- pop it
            curimg = image_stack.back ();
            image_stack.resize (image_stack.size()-1);
        } else {
            // Nothing on the stack, so get rid of the current image
            curimg = ImageRecRef();
        }
        return r;
    }

    ImageRecRef top () { return curimg; }

    void error (const std::string &command, const std::string &explanation="");

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

    // Initialize an ImageRec with a collection of prepared ImageSpec's.
    // The number of subimages is nsubimages, the number of MIP levels
    // for each subimages is in miplevels[0..nsubimages-1] (if miplevels
    // is NULL, allocate just one MIP level per subimage), and specs[]
    // contains the specs for all the MIP levels of subimage 0, followed
    // by all the specs for the MIP levels of subimage 1, and so on.
    // If spec == NULL, the IB's will not be fully allocated/initialized.
    ImageRec (const std::string &name, int nsubimages = 1,
              const int *miplevels = NULL, const ImageSpec *specs=NULL);

    // Copy an existing ImageRec.  Copy just the single subimage_to_copy
    // if >= 0, or all subimages if <0.  Copy just the single
    // miplevel_to_copy if >= 0, or all MIP levels if <0.  If writable
    // is true, we expect to need to alter the pixels of the resulting
    // ImageRec.  If copy_pixels is false, just make the new image big
    // enough, no need to initialize the pixel values.
    ImageRec (ImageRec &img, int subimage_to_copy = -1,
              int miplevel_to_copy = -1, bool writable = true,
              bool copy_pixels = true);

    // Create an ImageRef that consists of the ImageBuf img.  Copy img
    // if copy_pixels==true, otherwise just take ownership of img (it's
    // a shared pointer).
    ImageRec (ImageBufRef img, bool copy_pixels = true);

    // Initialize an ImageRec with the given spec.
    ImageRec (const std::string &name, const ImageSpec &spec,
              ImageCache *imagecache);

    enum WinMerge { WinMergeUnion, WinMergeIntersection, WinMergeA, WinMergeB };

    // Initialize a new ImageRec based on two exemplars.  Initialize
    // just the single subimage_to_copy if >= 0, or all subimages if <0.
    // The two WinMerge parameters pixwin and fullwin, dictate the
    // policy for setting up the pixel data and full (display) windows,
    // respectively.  If pixeltype not UNKNOWN, use that rather than
    // A's pixel type (the default behavior).
    ImageRec (ImageRec &imgA, ImageRec &imgB, int subimage_to_copy = -1,
              WinMerge pixwin = WinMergeUnion,
              WinMerge fullwin = WinMergeUnion,
              TypeDesc pixeltype = TypeDesc::UNKNOWN);

    // Number of subimages
    int subimages() const { return (int) m_subimages.size(); }

    // Number of MIP levels of the given subimage
    int miplevels (int subimage=0) const {
        if (subimage >= subimages())
            return 0;
        return m_subimages[subimage].miplevels();
    }

    // Subimage reference accessors.
    SubimageRec& subimage (int i) {
        return m_subimages[i];
    }
    const SubimageRec& subimage (int i) const {
        return m_subimages[i];
    }

    // Accessing it like an array returns a specific subimage
    SubimageRec& operator[] (int i) {
        return m_subimages[i];
    }
    const SubimageRec& operator[] (int i) const {
        return m_subimages[i];
    }

    std::string name () const { return m_name; }

    // Has the ImageRec been actually read or evaluated?  (Until needed,
    // it's lazily kept as name only, without reading the file.)
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

    // This should be called if for some reason the underlying
    // ImageBuf's spec may have been modified in place.  We need to
    // update the outer copy held by the SubimageRec.
    void update_spec_from_imagebuf (int subimg=0, int mip=0) {
        *m_subimages[subimg].spec(mip) = m_subimages[subimg][mip]->spec();
        metadata_modified();
    }

    /// Error reporting for ImageRec: call this with printf-like arguments.
    /// Note however that this is fully typesafe!
    /// void error (const char *format, ...)
    TINYFORMAT_WRAP_FORMAT (void, error, const,
        std::ostringstream msg;, msg, append_error(msg.str());)

    /// Return true if the IR has had an error and has an error message
    /// to retrieve via geterror().
    bool has_error (void) const;

    /// Return the text of all error messages issued since geterror() was
    /// called (or an empty string if no errors are pending).  This also
    /// clears the error message for next time if clear_error is true.
    std::string geterror (bool clear_error = true) const;

private:
    std::string m_name;
    bool m_elaborated;
    bool m_metadata_modified;
    bool m_pixels_modified;
    std::vector<SubimageRec> m_subimages;
    std::time_t m_time;  //< Modification time of the input file
    ImageCache *m_imagecache;
    mutable std::string m_err;

    // Add to the error message
    void append_error (string_view message) const;

};




struct print_info_options {
    bool verbose;
    bool filenameprefix;
    bool sum;
    bool subimages;
    bool compute_sha1;
    bool compute_stats;
    bool dumpdata;
    bool dumpdata_showempty;
    std::string metamatch;
    std::string nometamatch;
    size_t namefieldlength;

    print_info_options ()
        : verbose(false), filenameprefix(false), sum(false), subimages(false),
          compute_sha1(false), compute_stats(false), dumpdata(false),
          dumpdata_showempty(true), namefieldlength(20)
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

int do_action_diff (ImageRec &ir0, ImageRec &ir1, Oiiotool &options,
                    int perceptual = 0);



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
