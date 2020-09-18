// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


#pragma once

#include <functional>
#include <memory>

#include <boost/container/flat_set.hpp>

#include <OpenImageIO/color.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/timer.h>


OIIO_NAMESPACE_BEGIN
namespace OiioTool {

typedef int (*CallbackFunction)(int argc, const char* argv[]);

class ImageRec;
typedef std::shared_ptr<ImageRec> ImageRecRef;


/// Polycy hints for reading images
enum ReadPolicy {
    ReadDefault = 0,        //< Default: use cache, maybe convert to float.
                            //<   For "small" files, may bypass cache.
    ReadNative = 1,         //< Keep in native type, use cache if it supports
                            //<   the native type, bypass if not. May still
                            //<   bypass cache for "small" images.
    ReadNoCache = 2,        //< Bypass the cache regardless of size (beware!),
                            //<   but still subject to format conversion.
    ReadNativeNoCache = 3,  //< No cache, no conversion. Do it all now.
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
    bool autocc;       // automatically color correct
    bool autopremult;  // auto premult unassociated alpha input
    bool nativeread;   // force native data type reads
    bool printinfo_verbose;
    bool metamerge;  // Merge source input metadata into output
    int cachesize;
    int autotile;
    int frame_padding;
    bool eval_enable;  // Enable evaluation of expressions
    std::string full_command_line;
    std::string printinfo_metamatch;
    std::string printinfo_nometamatch;
    std::string printinfo_format;
    ImageSpec input_config;         // configuration options for reading
    std::string input_channel_set;  // Optional input channel set

    // Output options
    TypeDesc output_dataformat;  // Requested output data format
    std::map<std::string, std::string> output_channelformats;
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
    bool output_force_tiles;  // for debugging
    bool metadata_nosoftwareattrib;

    // Options for --diff
    float diff_warnthresh;
    float diff_warnpercent;
    float diff_hardwarn;
    float diff_failthresh;
    float diff_failpercent;
    float diff_hardfail;

    // Internal state
    ImageRecRef curimg;                    // current image
    std::vector<ImageRecRef> image_stack;  // stack of previous images
    std::map<std::string, ImageRecRef> image_labels;  // labeled images
    ImageCache* imagecache = nullptr;                 // back ptr to ImageCache
    ColorConfig colorconfig;                          // OCIO color config
    Timer total_runtime;
    Timer total_readtime { Timer::DontStartNow };
    Timer total_writetime { Timer::DontStartNow };
    double total_imagecache_readtime = 0.0;
    typedef std::map<std::string, double> TimingMap;
    TimingMap function_times;
    size_t peak_memory          = 0;
    int return_value            = EXIT_SUCCESS;  // oiiotool command return code
    int num_outputs             = 0;             // Count of outputs written
    int frame_number            = 0;
    bool enable_function_timing = true;
    bool input_config_set       = false;
    bool printed_info           = false;  // printed info at some point
    // Remember the first input dataformats we encountered
    TypeDesc first_input_dataformat;
    int first_input_dataformat_bits = 0;
    std::map<std::string, std::string> first_input_channelformats;

    Oiiotool();

    void clear_options();
    void clear_input_config();

    /// Force img to be read at this point.  Use this wrapper, don't directly
    /// call img->read(), because there's extra work done here specific to
    /// oiiotool.
    bool read(ImageRecRef img, ReadPolicy readpolicy = ReadDefault);
    // Read the current image
    bool read(ReadPolicy readpolicy = ReadDefault)
    {
        if (curimg)
            return read(curimg, readpolicy);
        return true;
    }

    /// Force partial read of image (if it hasn't been yet), just enough
    /// that the nativespec can be examined.
    bool read_nativespec(ImageRecRef img);

    // If required_images are not yet on the stack, then postpone this
    // call by putting it on the 'pending' list and return true.
    // Otherwise (if enough images are on the stack), return false.
    bool postpone_callback(int required_images, CallbackFunction func, int argc,
                           const char* argv[]);
    bool postpone_callback(int required_images, ArgParse::Action func,
                           cspan<const char*> argv);

    // Process any pending commands.
    void process_pending();

    CallbackFunction pending_callback() const { return m_pending_callback; }
    const char* pending_callback_name() const { return m_pending_argv[0]; }
    const ArgParse::Action& pending_action() const { return m_pending_action; }
    const char* pending_action_name() const { return m_pending_argv[0]; }

    void push(const ImageRecRef& img)
    {
        if (img) {
            if (curimg)
                image_stack.push_back(curimg);
            curimg = img;
        }
    }

    void push(ImageRec* newir) { push(ImageRecRef(newir)); }

    ImageRecRef pop()
    {
        ImageRecRef r = curimg;
        if (image_stack.size()) {
            // There are images on the full stack -- pop it
            curimg = image_stack.back();
            image_stack.resize(image_stack.size() - 1);
        } else {
            // Nothing on the stack, so get rid of the current image
            curimg = ImageRecRef();
        }
        return r;
    }

    ImageRecRef top() { return curimg; }

    // How many images are on the stack?
    int image_stack_depth() const
    {
        return curimg ? 1 + int(image_stack.size()) : 0;
    }

    // Parse geom in the form of "x,y" to retrieve a 2D integer position.
    bool get_position(string_view command, string_view geom, int& x, int& y);

    // Modify the resolution and/or offset according to what's in geom.
    // Valid geometries are WxH (resolution), +X+Y (offsets), WxH+X+Y
    // (resolution and offset).  If 'allow_scaling' is true, geometries of
    // S% (e.g. "50%") or just S (e.g., "1.2") will be accepted to scale the
    // existing width and height (rounding to the nearest whole number of
    // pixels.
    bool adjust_geometry(string_view command, int& w, int& h, int& x, int& y,
                         const char* geom, bool allow_scaling = false,
                         bool allow_size = true) const;

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
    string_view express(string_view str);

    // Given a command with perhaps optional modifiers (for example,
    // "--cmd:a=1:pi=3.14"), extract the options and insert them into a
    // ParamValueList. For example, having attribute "a" with value "1"
    // and attribute "pi" with value "3.14".
    static ParamValueList extract_options(string_view command);

    // Error base case -- single unformatted string.
    void error(string_view command, string_view message = "") const;
    void warning(string_view command, string_view message = "") const;

    // Formatted errors with printf-like notation
    template<typename... Args>
    void errorf(string_view command, const char* fmt, const Args&... args) const
    {
        error(command, Strutil::sprintf(fmt, args...));
    }

    template<typename... Args>
    void warningf(string_view command, const char* fmt,
                  const Args&... args) const
    {
        warning(command, Strutil::sprintf(fmt, args...));
    }

    // Formatted errors with std::format-like notation
    template<typename... Args>
    void errorfmt(string_view command, const char* fmt,
                  const Args&... args) const
    {
        error(command, Strutil::fmt::format(fmt, args...));
    }

    template<typename... Args>
    void warningfmt(string_view command, const char* fmt,
                    const Args&... args) const
    {
        warning(command, Strutil::fmt::format(fmt, args...));
    }

    size_t check_peak_memory()
    {
        size_t mem  = Sysutil::memory_used();
        peak_memory = std::max(peak_memory, mem);
        return mem;
    }

    static std::string format_read_error(string_view filename, std::string err)
    {
        if (!err.size())
            err = "unknown error";
        if (!Strutil::contains(err, filename))
            err = Strutil::sprintf("\"%s\": %s", filename, err);
        return err;
    }

private:
    CallbackFunction m_pending_callback;
    ArgParse::Action m_pending_action;
    int m_pending_argc;
    const char* m_pending_argv[4];

    void express_error(const string_view expr, const string_view s,
                       string_view explanation);

    bool express_parse_atom(const string_view expr, string_view& s,
                            std::string& result);
    bool express_parse_factors(const string_view expr, string_view& s,
                               std::string& result);
    bool express_parse_summands(const string_view expr, string_view& s,
                                std::string& result);

    std::string express_impl(string_view s);
};


typedef std::shared_ptr<ImageBuf> ImageBufRef;



class SubimageRec {
public:
    int miplevels() const { return (int)m_miplevels.size(); }
    ImageBuf* operator()() { return miplevels() ? m_miplevels[0].get() : NULL; }
    ImageBuf* operator[](int i)
    {
        return i < miplevels() ? m_miplevels[i].get() : NULL;
    }
    const ImageBuf* operator[](int i) const
    {
        return i < miplevels() ? m_miplevels[i].get() : NULL;
    }
    ImageSpec* spec(int i) { return i < miplevels() ? &m_specs[i] : NULL; }
    const ImageSpec* spec(int i) const
    {
        return i < miplevels() ? &m_specs[i] : NULL;
    }

    // was_direct_read describes whether this subimage has is unmodified in
    // content and pixel format (i.e. data type) since it was read from a
    // preexisting file on disk. We set it to true upon first read, and a
    // handful of operations that should preserve it, but for almost
    // everything else, we don't propagate the value (letting it lapse to
    // false for the result of most image operations).
    bool was_direct_read() const { return m_was_direct_read; }
    void was_direct_read(bool f) { m_was_direct_read = f; }

private:
    std::vector<ImageBufRef> m_miplevels;
    std::vector<ImageSpec> m_specs;
    bool m_was_direct_read
        = false;  ///< Guaranteed pixel data type unmodified since read
    friend class ImageRec;
};



/// ImageRec is conceptually similar to an ImageBuf, except that whereas an
/// IB is truly a single image, an ImageRec encapsulates multiple subimages,
/// and potentially MIPmap levels for each subimage.
class ImageRec {
public:
    ImageRec(const std::string& name, ImageCache* imagecache)
        : m_name(name)
        , m_imagecache(imagecache)
    {
    }

    // Initialize an ImageRec with a collection of prepared ImageSpec's.
    // The number of subimages is nsubimages, the number of MIP levels
    // for each subimages is in miplevels[0..nsubimages-1] (if miplevels
    // is NULL, allocate just one MIP level per subimage), and specs[]
    // contains the specs for all the MIP levels of subimage 0, followed
    // by all the specs for the MIP levels of subimage 1, and so on.
    // If spec == NULL, the IB's will not be fully allocated/initialized.
    ImageRec(const std::string& name, int nsubimages, cspan<int> miplevels,
             cspan<ImageSpec> specs = {});

    ImageRec(const std::string& name, int nsubimages = 1)
        : ImageRec(name, nsubimages, cspan<int>(), cspan<ImageSpec>())
    {
    }

    // Copy an existing ImageRec.  Copy just the single subimage_to_copy
    // if >= 0, or all subimages if <0.  Copy just the single
    // miplevel_to_copy if >= 0, or all MIP levels if <0.  If writable
    // is true, we expect to need to alter the pixels of the resulting
    // ImageRec.  If copy_pixels is false, just make the new image big
    // enough, no need to initialize the pixel values.
    ImageRec(ImageRec& img, int subimage_to_copy, int miplevel_to_copy,
             bool writable, bool copy_pixels = true);

    // Create an ImageRef that consists of the ImageBuf img.  Copy img
    // if copy_pixels==true, otherwise just take ownership of img (it's
    // a shared pointer).
    ImageRec(ImageBufRef img, bool copy_pixels = true);

    // Initialize an ImageRec with the given spec.
    ImageRec(const std::string& name, const ImageSpec& spec,
             ImageCache* imagecache);

    ImageRec(const ImageRec& copy) = delete;  // Disallow copy ctr

    enum WinMerge { WinMergeUnion, WinMergeIntersection, WinMergeA, WinMergeB };

    // Initialize a new ImageRec based on two exemplars.  Initialize
    // just the single subimage_to_copy if >= 0, or all subimages if <0.
    // The two WinMerge parameters pixwin and fullwin, dictate the
    // policy for setting up the pixel data and full (display) windows,
    // respectively.  If pixeltype not UNKNOWN, use that rather than
    // A's pixel type (the default behavior).
    ImageRec(ImageRec& imgA, ImageRec& imgB, int subimage_to_copy = -1,
             WinMerge pixwin = WinMergeUnion, WinMerge fullwin = WinMergeUnion,
             TypeDesc pixeltype = TypeDesc::UNKNOWN);

    // Number of subimages
    int subimages() const { return (int)m_subimages.size(); }

    // Number of MIP levels of the given subimage
    int miplevels(int subimage = 0) const
    {
        if (subimage >= subimages())
            return 0;
        return m_subimages[subimage].miplevels();
    }

    // Subimage reference accessors.
    SubimageRec& subimage(int i) { return m_subimages[i]; }
    const SubimageRec& subimage(int i) const { return m_subimages[i]; }

    // Accessing it like an array returns a specific subimage
    SubimageRec& operator[](int i) { return m_subimages[i]; }
    const SubimageRec& operator[](int i) const { return m_subimages[i]; }

    // Remove a subimage from the list
    void erase_subimage(int i) { m_subimages.erase(m_subimages.begin() + i); }

    std::string name() const { return m_name; }

    // Has the ImageRec been actually read or evaluated?  (Until needed,
    // it's lazily kept as name only, without reading the file.)
    bool elaborated() const { return m_elaborated; }

    // Read just enough to fill in the nativespecs
    bool read_nativespec();

    bool read(ReadPolicy readpolicy   = ReadDefault,
              string_view channel_set = "");

    // ir(subimg,mip) references a specific MIP level of a subimage
    // ir(subimg) references the first MIP level of a subimage
    // ir() references the first MIP level of the first subimage
    ImageBuf& operator()(int subimg = 0, int mip = 0)
    {
        return *m_subimages[subimg][mip];
    }
    const ImageBuf& operator()(int subimg = 0, int mip = 0) const
    {
        return *m_subimages[subimg][mip];
    }

    ImageSpec* spec(int subimg = 0, int mip = 0)
    {
        return subimg < subimages() ? m_subimages[subimg].spec(mip) : NULL;
    }
    const ImageSpec* spec(int subimg = 0, int mip = 0) const
    {
        return subimg < subimages() ? m_subimages[subimg].spec(mip) : NULL;
    }

    const ImageSpec* nativespec(int subimg = 0, int mip = 0) const
    {
        return subimg < subimages() ? &((*this)(subimg, mip).nativespec())
                                    : nullptr;
    }

    bool was_output() const { return m_was_output; }
    void was_output(bool val) { m_was_output = val; }
    bool metadata_modified() const { return m_metadata_modified; }
    void metadata_modified(bool mod)
    {
        m_metadata_modified = mod;
        if (mod)
            was_output(false);
    }
    bool pixels_modified() const { return m_pixels_modified; }
    void pixels_modified(bool mod)
    {
        m_pixels_modified = mod;
        if (mod)
            was_output(false);
    }

    std::time_t time() const { return m_time; }

    // Request that any eventual input reads be stored internally in this
    // format. UNKNOWN means to use the usual default logic.
    void input_dataformat(TypeDesc dataformat)
    {
        m_input_dataformat = dataformat;
    }

    // This should be called if for some reason the underlying
    // ImageBuf's spec may have been modified in place.  We need to
    // update the outer copy held by the SubimageRec.
    void update_spec_from_imagebuf(int subimg = 0, int mip = 0)
    {
        *m_subimages[subimg].spec(mip) = m_subimages[subimg][mip]->spec();
        metadata_modified(true);
    }

    // Get or set the configuration spec that will be used any time the
    // image is opened.
    const ImageSpec* configspec() const { return m_configspec.get(); }
    void configspec(const ImageSpec& spec)
    {
        m_configspec.reset(new ImageSpec(spec));
    }
    void clear_configspec() { m_configspec.reset(); }

    /// Error reporting for ImageRec: call this with printf-like arguments.
    /// Note however that this is fully typesafe!
    template<typename... Args>
    void errorf(const char* fmt, const Args&... args) const
    {
        append_error(Strutil::sprintf(fmt, args...));
    }

    /// Return true if the IR has had an error and has an error message
    /// to retrieve via geterror().
    bool has_error(void) const;

    /// Return the text of all error messages issued since geterror() was
    /// called (or an empty string if no errors are pending).  This also
    /// clears the error message for next time if clear_error is true.
    std::string geterror(bool clear_error = true) const;

private:
    std::string m_name;
    bool m_elaborated        = false;
    bool m_metadata_modified = false;
    bool m_pixels_modified   = false;
    bool m_was_output        = false;
    std::vector<SubimageRec> m_subimages;
    std::time_t m_time;  //< Modification time of the input file
    TypeDesc m_input_dataformat;
    ImageCache* m_imagecache = nullptr;
    mutable std::string m_err;
    std::unique_ptr<ImageSpec> m_configspec;

    // Add to the error message
    void append_error(string_view message) const;
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

    print_info_options()
        : verbose(false)
        , filenameprefix(false)
        , sum(false)
        , subimages(false)
        , compute_sha1(false)
        , compute_stats(false)
        , dumpdata(false)
        , dumpdata_showempty(true)
        , namefieldlength(20)
    {
    }
};


// Print info about the named file to stdout, using print_info_options
// opt for guidance on what to print and how to do it.  The total size
// of the uncompressed pixels in the file is returned in totalsize.  The
// return value will be true if everything is ok, or false if there is
// an error (in which case the error message will be stored in 'error').
bool
print_info(Oiiotool& ot, const std::string& filename,
           const print_info_options& opt, std::string& error);


// Set an attribute of the given image.  The type should be one of
// TypeDesc::INT (decode the value as an int), FLOAT, STRING, or UNKNOWN
// (look at the string and try to discern whether it's an int, float, or
// string).  If the 'value' string is empty, it will delete the
// attribute.  If allsubimages is true, apply the attribute to all
// subimages, otherwise just the first subimage.
bool
set_attribute(ImageRecRef img, string_view attribname, TypeDesc type,
              string_view value, bool allsubimages);

inline bool
same_size(const ImageBuf& A, const ImageBuf& B)
{
    const ImageSpec &a(A.spec()), &b(B.spec());
    return (a.width == b.width && a.height == b.height && a.depth == b.depth
            && a.nchannels == b.nchannels);
}


enum DiffErrors {
    DiffErrOK = 0,         ///< No errors, the images match exactly
    DiffErrWarn,           ///< Warning: the errors differ a little
    DiffErrFail,           ///< Failure: the errors differ a lot
    DiffErrDifferentSize,  ///< Images aren't even the same size
    DiffErrFile,           ///< Could not find or open input files, etc.
    DiffErrLast
};

int
do_action_diff(ImageRec& ir0, ImageRec& ir1, Oiiotool& options,
               int perceptual = 0);

bool
decode_channel_set(const ImageSpec& spec, string_view chanlist,
                   std::vector<std::string>& newchannelnames,
                   std::vector<int>& channels, std::vector<float>& values);



// Helper template -- perform the action on each spec in the ImageRec.
// The action needs a signature like:
//     bool action(ImageSpec &spec, const T& t))
template<class Action, class Type>
bool
apply_spec_mod(ImageRec& img, Action act, const Type& t, bool allsubimages)
{
    bool ok = true;
    img.read();
    img.metadata_modified(true);
    for (int s = 0, send = img.subimages(); s < send; ++s) {
        for (int m = 0, mend = img.miplevels(s); m < mend; ++m) {
            ok &= act(img(s, m).specmod(), t);
            if (ok)
                img.update_spec_from_imagebuf(s, m);
            if (!allsubimages)
                break;
        }
        if (!allsubimages)
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
    using setup_func_t = std::function<bool(OiiotoolOp& op)>;
    using impl_func_t = std::function<bool(OiiotoolOp& op, span<ImageBuf*> img)>;
    using new_output_imagerec_func_t
        = std::function<ImageRecRef(OiiotoolOp& op)>;

    // The constructor records the arguments (including running them
    // through expression substitution) and pops the input images off the
    // stack.
    OiiotoolOp(Oiiotool& ot, string_view opname, int argc, const char* argv[],
               int ninputs, setup_func_t setup_func, impl_func_t impl_func)
        : ot(ot)
        , m_nargs(argc)
        , m_nimages(ninputs + 1)
        , m_setup_func(setup_func)
        , m_impl_func(impl_func)
    {
        if (Strutil::starts_with(opname, "--"))
            opname.remove_prefix(1);  // canonicalize to one dash
        m_opname = opname.substr(0, opname.find_first_of(':'));  // and no :
        m_args.reserve(argc);
        for (int i = 0; i < argc; ++i)
            m_args.push_back(ot.express(argv[i]));
        m_ir.resize(ninputs + 1);  // including reserving a spot for result
        for (int i = 0; i < ninputs; ++i)
            m_ir[ninputs - i] = ot.pop();
    }
    OiiotoolOp(Oiiotool& ot, string_view opname, int argc, const char* argv[],
               int ninputs, impl_func_t impl_func = {})
        : OiiotoolOp(ot, opname, argc, argv, ninputs, {}, impl_func)
    {
    }
    virtual ~OiiotoolOp() {}

    // The operator(), function-call mode, does most of the work. Although
    // it's virtual, in general you shouldn't need to override it. Instead,
    // just override impl() or supply an impl_func at construction.
    virtual int operator()()
    {
        // Set up a timer to automatically record how much time is spent in
        // every class of operation.
        Timer timer(ot.enable_function_timing);
        if (ot.debug) {
            std::cout << "Performing '" << opname() << "'";
            if (nargs() > 1)
                std::cout << " with args: ";
            for (int i = 0; i < nargs(); ++i)
                std::cout << (i > 0 ? ", \"" : " \"") << m_args[i] << "\"";
            std::cout << "\n";
        }

        // Parse the options.
        m_options.clear();
        m_options["allsubimages"] = (int)ot.allsubimages;
        m_options                 = ot.extract_options(m_args[0]);

        // Read all input images, and reserve (and push) the output image.
        int subimages = compute_subimages();
        for (int i = 1; i < nimages(); ++i)
            ot.read(m_ir[i]);
        if (nimages()) {
            // Read the inputs
            subimages = compute_subimages();
            // Initialize the output image
            m_ir[0] = new_output_imagerec();
            ot.push(m_ir[0]);
        }

        // Give a chance for customization before we walk the subimages.
        // If the setup method returns false, we're done.
        if (!setup()) {
            return 0;
        } else {
            if (skip_impl()) {
                // setup must have asked to skip the rest of the impl.
                // Just copy the input instead.
                if (nimages())
                    m_ir[0] = m_ir[1];
            } else {
                traverse_subimages(subimages);
            }
        }

        if (ot.debug || ot.runstats)
            ot.check_peak_memory();

        // Optional cleanup after processing all the subimages
        cleanup();

        // Add the time we spent to the stats total for this op type.
        double optime = timer();
        ot.function_times[opname()] += optime;
        if (ot.debug) {
            Strutil::printf("    %s took %s  (total time %s, mem %s)\n",
                            opname(), Strutil::timeintervalformat(optime, 2),
                            Strutil::timeintervalformat(ot.total_runtime(), 2),
                            Strutil::memformat(Sysutil::memory_used()));
        }
        return 0;
    }

    virtual void traverse_subimages(int subimages)
    {
        // For each subimage, find the ImageBuf's for input and output
        // images, and call impl().
        for (int s = 0; s < subimages; ++s) {
            // Get pointers for the ImageBufs for this subimage
            m_img.resize(nimages());
            for (int m = 0, nmip = ir(0)->miplevels(); m < nmip; ++m) {
                for (int i = 0; i < nimages(); ++i)
                    m_img[i] = &((*ir(i))(std::min(s, ir(i)->subimages() - 1),
                                          std::min(m, ir(i)->miplevels(s))));

                if (subimage_is_active(s)) {
                    // Call the impl kernel for this subimage
                    bool ok = impl(m_img);
                    if (!ok)
                        ot.errorf(opname(), "%s", m_img[0]->geterror());

                    // Merge metadata if called for
                    if (ot.metamerge)
                        for (int i = 1; i < nimages(); ++i)
                            m_img[0]->specmod().extra_attribs.merge(
                                m_img[i]->spec().extra_attribs);
                } else {
                    // Inactive subimage, just copy.
                    if (nimages() >= 2)
                        m_img[0]->copy(*m_img[1]);
                }
                m_ir[0]->update_spec_from_imagebuf(s, m);
            }

            // Make sure to forward any errors missed by the impl
            for (auto& im : m_img)
                if (im->has_error())
                    ot.errorf(opname(), "%s", im->geterror());
        }
    }

    // THIS is the method that needs to be separately overloaded for each
    // different op. This is called once for each subimage, generally with
    // img[0] the destination ImageBuf, and img[1..] as the inputs. It's
    // also possible to override just this by supplying the impl_func,
    // without needing to subclass at all. The default is to copy the first
    // input image.
    virtual bool impl(span<ImageBuf*> img)
    {
        if (m_impl_func) {
            return m_impl_func(*this, img);
        } else {
            return m_img.size() > 1 ? img[0]->copy(*img[1]) : false;
        }
    }

    // Extra place to inject customization before the subimages are
    // traversed. It's also possible to override just this by supplying the
    // setup_func, without needing to subclass at all.
    virtual bool setup() { return m_setup_func ? m_setup_func(*this) : true; }

    // Return an ImageRecRef of the new output image. The default just
    // makes an ImageRecRef with enough slots for the number of subimages
    // that can be discerned from the inputs. This can be overloaded for
    // custom behavior of subclasses.
    virtual ImageRecRef new_output_imagerec()
    {
        if (m_new_output_imagerec_func) {
            // Callback supplied -- use it.
            return m_new_output_imagerec_func(*this);
        }
        // No callback, we're on our own
        if (preserve_miplevels()) {
            std::vector<int> allmiplevels;
            for (int s = 0, se = compute_subimages(); s < se; ++s)
                allmiplevels.push_back(ir(1)->miplevels(s));
            return std::make_shared<ImageRec>(ir(1)->name(),
                                              (int)allmiplevels.size(),
                                              allmiplevels);
        } else {
            // Not instructed to preserve MIP levels. Just copy from the
            // input image.
            return std::make_shared<ImageRec>(opname(), compute_subimages());
        }
    }

    // Extra place to inject customization after the subimages are traversed.
    virtual bool cleanup() { return true; }

    // Default subimage logic: if the global -a flag was set or if this
    // command had ":allsubimages=1" option set, then apply the command to
    // all subimages (of the first input image). Otherwise, we'll only apply
    // the command to the first subimage. Override this if is you want
    // another behavior. Also, this sets up the include/exclude list for
    // subimages based on optional ":subimages=...". The subimage list is
    // comma separate list of "all", subimage index, or negative subimage
    // index (which meant to exclude that index). If a subimage list is
    // supplied, it also implies "allsubimages".
    virtual int compute_subimages()
    {
        subimage_includes.clear();
        subimage_excludes.clear();
        int all_subimages = 0;
        auto sispec = Strutil::splitsv(m_options.get_string("subimages"), ",");
        for (auto s : sispec) {
            Strutil::trim_whitespace(s);
            bool exclude       = Strutil::parse_char(s, '-');
            int named_subimage = -1;
            if (s.size() == 0)
                continue;
            if (Strutil::string_is_int(s)) {
                int si = Strutil::from_string<int>(s);
                if (exclude)
                    subimage_excludes.insert(si);
                else
                    subimage_includes.insert(si);
                all_subimages = 1;
            } else if ((named_subimage = subimage_index(s)) >= 0) {
                if (exclude)
                    subimage_excludes.insert(named_subimage);
                else
                    subimage_includes.insert(named_subimage);
                all_subimages = 1;
            } else if (s == "all") {
                subimage_includes.clear();
                subimage_excludes.clear();
                all_subimages = 1;
            }
        }
        all_subimages |= m_options.get_int("allsubimages", ot.allsubimages);
        return all_subimages ? (nimages() > 1 ? ir(1)->subimages() : 1) : 1;
    }

    // Is the given subimage in the active set to be operated on by this op?
    // It is if it's in the include set, but not the exclude set. Empty
    // include set means "include all", empty exclude set means "exclude
    // none."
    virtual bool subimage_is_active(int s)
    {
        return (subimage_includes.size() == 0
                || subimage_includes.find(s) != subimage_includes.end())
               && (subimage_excludes.size() == 0
                   || subimage_excludes.find(s) == subimage_excludes.end());
        return true;
    }

    int subimage_index(string_view name)
    {
        // For each image on the stack, check if the names of any of its
        // subimages is a match.
        for (int i = 0; i < nimages(); ++i) {
            if (!ir(i))
                continue;
            for (int s = 0; s < ir(i)->subimages(); ++s) {
                const ImageSpec* spec = ir(i)->spec(s);
                if (spec
                    && spec->get_string_attribute("oiio:subimagename") == name)
                    return s;
            }
        }
        return -1;
    }

    int nargs() const { return m_nargs; }
    string_view args(int i) const { return m_args[i]; }
    int nimages() const { return m_nimages; }
    string_view opname() const { return m_opname; }
    const ParamValueList& options() const { return m_options; }
    ImageBuf* img(int i) const { return m_img[i]; }

    // Retrieve an ImageRec we're working on. (Note: [0] is the output.)
    ImageRecRef& ir(int i) { return m_ir[i]; }
    const ImageRecRef& ir(int i) const { return m_ir[i]; }

    // Set a customized setup() function
    void set_setup(setup_func_t func) { m_setup_func = func; }

    // Set a customized impl() function
    void set_impl(impl_func_t func) { m_impl_func = func; }

    // Set a customized new_output_imagerec() function
    void set_new_output_imagerec(new_output_imagerec_func_t func)
    {
        m_new_output_imagerec_func = func;
    }

    // Call preserve_miplevels(true) if the impl should traverse all MIP
    // levels.
    void preserve_miplevels(bool val) { m_preserve_miplevels = val; }
    bool preserve_miplevels() const { return m_preserve_miplevels; }

    // Call skip_impl(true) if the impl should skipped entirely and just
    // leave the stack unchanged. This can be set by a custom setup method.
    void skip_impl(bool val) { m_skip_impl = val; }
    bool skip_impl() const { return m_skip_impl; }

protected:
    Oiiotool& ot;
    std::string m_opname;
    int m_nargs;
    int m_nimages;
    bool m_preserve_miplevels = false;
    bool m_skip_impl          = false;
    std::vector<ImageRecRef> m_ir;
    std::vector<ImageBuf*> m_img;
    std::vector<string_view> m_args;
    ParamValueList m_options;
    typedef boost::container::flat_set<int> FastIntSet;
    FastIntSet subimage_includes;  // Subimages to operate on (empty == all)
    FastIntSet subimage_excludes;  // Subimages to skip for the op
    setup_func_t m_setup_func;
    impl_func_t m_impl_func;
    new_output_imagerec_func_t m_new_output_imagerec_func;
};


}  // namespace OiioTool
OIIO_NAMESPACE_END;
