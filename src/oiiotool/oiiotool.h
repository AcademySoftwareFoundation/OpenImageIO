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

#include <memory>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/sysutil.h>


OIIO_NAMESPACE_BEGIN
namespace OiioTool {

typedef int (*CallbackFunction)(int argc,const char*argv[]);

class ImageRec;
typedef std::shared_ptr<ImageRec> ImageRecRef;


/// Polycy hints for reading images
enum ReadPolicy {
    ReadDefault = 0,       //< Default: use cache, maybe convert to float.
                           //<   For "small" files, may bypass cache.
    ReadNative  = 1,       //< Keep in native type, use cache if it supports
                           //<   the native type, bypass if not. May still
                           //<   bypass cache for "small" images.
    ReadNoCache = 2,       //< Bypass the cache regardless of size (beware!),
                           //<   but still subject to format conversion.
    ReadNativeNoCache = 3, //< No cache, no conversion. Do it all now.
                           //<   You better know what you're doing.
};



class Oiiotool {
public:
    // General options
    bool verbose;
    bool debug;
    bool dryrun;
    bool runstats;
    bool noclobber;
    bool allsubimages;
    bool printinfo;
    bool printstats;
    bool dumpdata;
    bool dumpdata_showempty;
    bool hash;
    bool updatemode;
    bool autoorient;
    bool autocc;                      // automatically color correct
    bool nativeread;                  // force native data type reads
    bool printinfo_verbose;
    int cachesize;
    int autotile;
    int frame_padding;
    std::string full_command_line;
    std::string printinfo_metamatch;
    std::string printinfo_nometamatch;
    std::string printinfo_format;
    ImageSpec input_config;           // configuration options for reading
    std::string input_channel_set;    // Optional input channel set

    // Output options
    TypeDesc output_dataformat;       // Requested output data format
    std::map<std::string,std::string> output_channelformats;
    std::string output_compression;
    std::string output_planarconfig;
    int output_bitspersample;
    int output_tilewidth, output_tileheight;
    int output_quality;
    bool output_scanline;
    bool output_adjust_time;
    bool output_autocrop;
    bool output_autotrim;
    bool output_dither;
    bool output_force_tiles; // for debugging
    bool metadata_nosoftwareattrib;

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
    ImageCache *imagecache = nullptr;        // back ptr to ImageCache
    ColorConfig colorconfig;                 // OCIO color config
    Timer total_runtime;
    Timer total_readtime  {Timer::DontStartNow};
    Timer total_writetime {Timer::DontStartNow};
    double total_imagecache_readtime = 0.0;
    typedef std::map<std::string, double> TimingMap;
    TimingMap function_times;
    size_t peak_memory = 0;
    int return_value = EXIT_SUCCESS;         // oiiotool command return code
    int num_outputs = 0;                     // Count of outputs written
    int frame_number = 0;
    bool enable_function_timing = true;
    bool input_config_set = false;
    bool printed_info = false;               // printed info at some point
    // Remember the first input dataformats we encountered
    TypeDesc first_input_dataformat;
    int first_input_dataformat_bits = 0;
    std::map<std::string, std::string> first_input_channelformats;

    Oiiotool ();

    void clear_options ();

    /// Force img to be read at this point.  Use this wrapper, don't directly
    /// call img->read(), because there's extra work done here specific to
    /// oiiotool.
    bool read (ImageRecRef img, ReadPolicy readpolicy = ReadDefault);
    // Read the current image
    bool read (ReadPolicy readpolicy = ReadDefault) {
        if (curimg)
            return read (curimg, readpolicy);
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

    // How many images are on the stack?
    int image_stack_depth () const {
        return curimg ? 1+int(image_stack.size()) : 0;
    }

    // Parse geom in the form of "x,y" to retrieve a 2D integer position.
    bool get_position (string_view command, string_view geom, int &x, int &y);

    // Modify the resolution and/or offset according to what's in geom.
    // Valid geometries are WxH (resolution), +X+Y (offsets), WxH+X+Y
    // (resolution and offset).  If 'allow_scaling' is true, geometries of
    // S% (e.g. "50%") or just S (e.g., "1.2") will be accepted to scale the
    // existing width and height (rounding to the nearest whole number of
    // pixels.
    bool adjust_geometry (string_view command,
                          int &w, int &h, int &x, int &y, const char *geom,
                          bool allow_scaling=false) const;

    // Expand substitution expressions in string str. Expressions are
    // enclosed in braces: {...}. An expression consists of:
    //   * a numeric constant ("42" or "3.14")
    //   * arbitrary math using operators +, -, *, / and parentheses
    //     (order of operations is respected).
    //   * IMG[n].metadata for the metadata of an image. The 'n' may be an
    //     image name, or an integer giving stack position (for example,
    //     "IMG[0]" is the top of the stack; also "TOP" is a synonym). The
    //     metadata can be any of the usual named metadata from the image's
    //     spec, such as "width", "ImageDescription", etc.
    string_view express (string_view str);

    int extract_options (std::map<std::string,std::string> &options,
                         std::string command);

    void error (string_view command, string_view explanation="") const;
    void warning (string_view command, string_view explanation="") const;

    size_t check_peak_memory () {
        size_t mem = Sysutil::memory_used();
        peak_memory = std::max (peak_memory, mem);
        return mem;
    }

private:
    CallbackFunction m_pending_callback;
    int m_pending_argc;
    const char *m_pending_argv[4];

    void express_error (const string_view expr, const string_view s, string_view explanation);

    bool express_parse_atom (const string_view expr, string_view& s, std::string& result);
    bool express_parse_factors (const string_view expr, string_view& s, std::string& result);
    bool express_parse_summands (const string_view expr, string_view& s, std::string& result);

    std::string express_impl (string_view s);
};


typedef std::shared_ptr<ImageBuf> ImageBufRef;



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

    // was_direct_read describes whether this subimage has is unomdified in
    // content and pixel format (i.e. data type) since it was read from a
    // preexisting file on disk. We set it to true upon first read, and a
    // handful of operations that should preserve it, but for almost
    // everything else, we don't propagate the value (letting it lapse to
    // false for the result of most image operations).
    bool was_direct_read () const { return m_was_direct_read; }
    void was_direct_read (bool f) { m_was_direct_read = f; }

private:
    std::vector<ImageBufRef> m_miplevels;
    std::vector<ImageSpec> m_specs;
    bool m_was_direct_read = false;  ///< Guaranteed pixel data type unmodified since read
    friend class ImageRec;
};



/// ImageRec is conceptually similar to an ImageBuf, except that whereas an
/// IB is truly a single image, an ImageRec encapsulates multiple subimages,
/// and potentially MIPmap levels for each subimage.
class ImageRec {
public:
    ImageRec (const std::string &name, ImageCache *imagecache)
        : m_name(name), m_imagecache(imagecache)
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

    ImageRec (const ImageRec &copy) = delete;  // Disallow copy ctr

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

    bool read (ReadPolicy readpolicy = ReadDefault,
               string_view channel_set = "");

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

    const ImageSpec * nativespec (int subimg=0, int mip=0) const {
        return subimg < subimages() ? &((*this)(subimg,mip).nativespec()) : nullptr;
    }

    bool was_output () const { return m_was_output; }
    void was_output (bool val) { m_was_output = val; }
    bool metadata_modified () const { return m_metadata_modified; }
    void metadata_modified (bool mod) {
        m_metadata_modified = mod;
        if (mod)
            was_output(false);
    }
    bool pixels_modified () const { return m_pixels_modified; }
    void pixels_modified (bool mod) {
        m_pixels_modified = mod;
        if (mod)
            was_output(false);
    }

    std::time_t time() const { return m_time; }

    // Request that any eventual input reads be stored internally in this
    // format. UNKNOWN means to use the usual default logic.
    void input_dataformat (TypeDesc dataformat) {
        m_input_dataformat = dataformat;
    }

    // This should be called if for some reason the underlying
    // ImageBuf's spec may have been modified in place.  We need to
    // update the outer copy held by the SubimageRec.
    void update_spec_from_imagebuf (int subimg=0, int mip=0) {
        *m_subimages[subimg].spec(mip) = m_subimages[subimg][mip]->spec();
        metadata_modified (true);
    }

    // Get or set the configuration spec that will be used any time the
    // image is opened.
    const ImageSpec * configspec () const { return &m_configspec; }
    void configspec (const ImageSpec &spec) { m_configspec = spec; }
    void clear_configspec () { configspec (ImageSpec()); }

    /// Error reporting for ImageRec: call this with printf-like arguments.
    /// Note however that this is fully typesafe!
    template<typename... Args>
    void error (string_view fmt, const Args&... args) const {
        append_error(Strutil::format (fmt, args...));
    }

    /// Return true if the IR has had an error and has an error message
    /// to retrieve via geterror().
    bool has_error (void) const;

    /// Return the text of all error messages issued since geterror() was
    /// called (or an empty string if no errors are pending).  This also
    /// clears the error message for next time if clear_error is true.
    std::string geterror (bool clear_error = true) const;

private:
    std::string m_name;
    bool m_elaborated = false;
    bool m_metadata_modified = false;
    bool m_pixels_modified = false;
    bool m_was_output = false;
    std::vector<SubimageRec> m_subimages;
    std::time_t m_time;  //< Modification time of the input file
    TypeDesc m_input_dataformat;
    ImageCache *m_imagecache = nullptr;
    mutable std::string m_err;
    ImageSpec m_configspec;

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
    std::string infoformat;
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
bool print_info (Oiiotool &ot, const std::string &filename, 
                 const print_info_options &opt,
                 long long &totalsize, std::string &error);


// Set an attribute of the given image.  The type should be one of
// TypeDesc::INT (decode the value as an int), FLOAT, STRING, or UNKNOWN
// (look at the string and try to discern whether it's an int, float, or
// string).  If the 'value' string is empty, it will delete the
// attribute.  If allsubimages is true, apply the attribute to all
// subimages, otherwise just the first subimage.
bool set_attribute (ImageRecRef img, string_view attribname,
                    TypeDesc type, string_view value,
                    bool allsubimages);

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

bool decode_channel_set (const ImageSpec &spec, string_view chanlist,
                    std::vector<std::string> &newchannelnames,
                    std::vector<int> &channels, std::vector<float> &values);



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
            ok &= act (img(s,m).specmod(), t);
            if (ok)
                img.update_spec_from_imagebuf (s, m);
            if (! allsubimages)
                break;
        }
        if (! allsubimages)
            break;
    }
    return ok;
}



/// Base class for an Oiiotool operation/command. Rather than repeating
/// code, this provides the boilerplate that nearly every op must do,
/// with just a couple tiny places that need to be overridden for each op,
/// generally only the impl() method.
///
class OiiotoolOp {
public:
    // The constructor records the arguments (including running them
    // through expression substitution) and pops the input images off the
    // stack.
    OiiotoolOp (Oiiotool &ot, string_view opname,
                int argc, const char *argv[], int ninputs)
        : ot(ot), m_opname(opname), m_nargs(argc), m_nimages(ninputs+1)
    {
        args.reserve (argc);
        for (int i = 0; i < argc; ++i)
            args.push_back (ot.express (argv[i]));
        ir.resize (ninputs+1);  // including reserving a spot for result
        for (int i = 0; i < ninputs; ++i)
            ir[ninputs-i] = ot.pop();
    }
    virtual ~OiiotoolOp () {}

    // The operator(), function-call mode, does most of the work. Although
    // it's virtual, in general you shouldn't need to override it. Instead,
    // just override impl(), and maybe option_defaults.
    virtual int operator() () {
        // Set up a timer to automatically record how much time is spent in
        // every class of operation.
        Timer timer (ot.enable_function_timing);
        if (ot.debug) {
            std::cout << "Performing '" << opname() << "'";
            if (nargs() > 1)
                std::cout << " with args: ";
            for (int i = 0; i < nargs(); ++i)
                std::cout << (i > 0 ? ", \"" : " \"") << args[i] << "\"";
            std::cout << "\n";
        }

        // Parse the options.
        options.clear ();
        options["allsubimages"] = ot.allsubimages;
        option_defaults ();  // this can be customized to set up defaults
        ot.extract_options (options, args[0]);

        // Read all input images, and reserve (and push) the output image.
        int subimages = compute_subimages();
        if (nimages()) {
            // Read the inputs
            for (int i = 1; i < nimages(); ++i)
                ot.read (ir[i]);
            // Initialize the output image
            ir[0].reset (new ImageRec (opname(), subimages));
            ot.push (ir[0]);
        }

        // Give a chance for customization before we walk the subimages.
        // If the setup method returns false, we're done.
        if (! setup ())
            return 0;

        // For each subimage, find the ImageBuf's for input and output
        // images, and call impl().
        for (int s = 0;  s < subimages;  ++s) {
            // Get pointers for the ImageBufs for this subimage
            img.resize (nimages());
            for (int i = 0; i < nimages(); ++i)
                img[i] = &((*ir[i])(std::min (s, ir[i]->subimages()-1)));

            // Call the impl kernel for this subimage
            bool ok = impl (nimages() ? &img[0] : NULL);
            if (! ok)
                ot.error (opname(), img[0]->geterror());
            ir[0]->update_spec_from_imagebuf (s);
        }

        // Make sure to forward any errors missed by the impl
        for (int i = 0; i < nimages(); ++i) {
            if (img[i]->has_error())
                ot.error (opname(), img[i]->geterror());
        }

        if (ot.debug || ot.runstats)
            ot.check_peak_memory();

        // Optional cleanup after processing all the subimages
        cleanup ();

        // Add the time we spent to the stats total for this op type.
        double optime = timer();
        ot.function_times[opname()] += optime;
        if (ot.debug) {
            Strutil::printf ("    %s took %s  (total time %s, mem %s)\n",
                             opname(), Strutil::timeintervalformat(optime,2),
                             Strutil::timeintervalformat(ot.total_runtime(),2),
                             Strutil::memformat(Sysutil::memory_used()));
        }
        return 0;
    }

    // THIS is the method that needs to be separately overloaded for each
    // different op. This is called once for each subimage, generally with
    // img[0] the destination ImageBuf, and img[1..] as the inputs.
    virtual int impl (ImageBuf **img) = 0;

    // Extra place to inject customization before the subimages are
    // traversed.
    virtual bool setup () { return true; }

    // Extra place to inject customization after the subimges are traversed.
    virtual bool cleanup () { return true; }

    // Override this if the impl uses options and needs any of them set
    // to defaults. This will be called separate
    virtual void option_defaults () { }

    // Default subimage logic: if the global -a flag was set or if this command
    // had ":allsubimages=1" option set, then apply the command to all subimages
    // (of the first input image). Otherwise, we'll only apply the command to
    // the first subimage. Override this is you want another behavior.
    virtual int compute_subimages () {
        int all_subimages = Strutil::from_string<int>(options["allsubimages"]);
        return all_subimages ? (nimages() > 1 ? ir[1]->subimages() : 1) : 1;
    }

    int nargs () const { return m_nargs; }
    int nimages () const { return m_nimages; }
    string_view opname () const { return m_opname; }

protected:
    Oiiotool &ot;
    std::string m_opname;
    int m_nargs;
    int m_nimages;
    std::vector<ImageRecRef> ir;
    std::vector<ImageBuf *> img;
    std::vector<string_view> args;
    std::map<std::string,std::string> options;
};


typedef bool (*IBAunary) (ImageBuf &dst, const ImageBuf &A, ROI roi, int nthreads);
typedef bool (*IBAbinary) (ImageBuf &dst, const ImageBuf &A,
                           const ImageBuf &B, ROI roi, int nthreads);
typedef bool (*IBAbinary_img_col) (ImageBuf &dst, const ImageBuf &A,
                                   const float *B, ROI roi, int nthreads);

template<typename IBLIMPL=IBAunary>
class OiiotoolSimpleUnaryOp : public OiiotoolOp {
public:
    OiiotoolSimpleUnaryOp (IBLIMPL opimpl, Oiiotool &ot, string_view opname,
                           int argc, const char *argv[], int ninputs)
        : OiiotoolOp (ot, opname, argc, argv, 1), opimpl(opimpl)
    {}
    virtual int impl (ImageBuf **img) {
        return opimpl (*img[0], *img[1], ROI(), 0);
    }
protected:
    IBLIMPL opimpl;
};

template<typename IBLIMPL=IBAbinary>
class OiiotoolSimpleBinaryOp : public OiiotoolOp {
public:
    OiiotoolSimpleBinaryOp (IBLIMPL opimpl, Oiiotool &ot, string_view opname,
                            int argc, const char *argv[], int ninputs)
        : OiiotoolOp (ot, opname, argc, argv, 2), opimpl(opimpl)
    {}
    virtual int impl (ImageBuf **img) {
        return opimpl (*img[0], *img[1], *img[2], ROI(), 0);
    }
protected:
    IBLIMPL opimpl;
};

template<typename IBLIMPL=IBAbinary_img_col>
class OiiotoolImageColorOp : public OiiotoolOp {
public:
    OiiotoolImageColorOp (IBLIMPL opimpl, Oiiotool &ot, string_view opname,
                          int argc, const char *argv[], int ninputs,
                          float defaultval=0.0f)
        : OiiotoolOp (ot, opname, argc, argv, 1), opimpl(opimpl),
          defaultval(defaultval)
    {}
    virtual int impl (ImageBuf **img) {
        int nchans = img[1]->spec().nchannels;
        std::vector<float> val (nchans, defaultval);
        int nvals = Strutil::extract_from_list_string (val, args[1]);
        val.resize (nvals);
        val.resize (nchans, val.size() == 1 ? val.back() : defaultval);
        return opimpl (*img[0], *img[1], &val[0], ROI(), 0);
    }
protected:
    IBLIMPL opimpl;
    float defaultval;
};


} // OiioTool namespace
OIIO_NAMESPACE_END;


#endif // OIIOTOOL_H
