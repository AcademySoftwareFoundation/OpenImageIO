// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

#ifndef OPENIMAGEIO_NSVERSIONS_H
#define OPENIMAGEIO_NSVERSIONS_H

#ifndef OPENIMAGEIO_VERSION_H
#    error "oiioversion.h must always be included before nsversions.h"
#endif


// Establish the namespaces.
//
// The outer namespace defaults to OpenImageIO, but can be overriden at build
// time. "OIIO" is always defined as an alias to this namespace, so client
// software can always say `OIIO:Foo` without needing to know the custom outer
// namespace.
//
// The primary inner namespace is vMAJ_MIN (or in main, vMAJ_MIN_PAT). The
// outer namespace declares the inner namespace as "inline", so anything in it
// is visible in OIIO by default.
//
// But we also keep around all the symbols for older vOLDMAJ_OLDMIN, so that
// we don't lose link/ABI compatibility with apps compiled for older minor
// versions within the same major version family.
//
//
// Here is the scheme we use to maintain ABI compatibility across minor
// version boundaries.
//
// 1. Declarations in the current new namespace
//
//    * Pure OIIO internals that are not exposed in the public APIs or
//      headers, and therefore generate no externally visible linker symbols.
//    * NEW items that will break ABI or API, in main (will be moved into a
//      specific version namespace before release).
//
//    These declarations perpetually live in the "current" namespace:
//
//        OIIO_NAMESPACE_BEGIN
//            inline int myfunc() { ... }
//            constexpr int myconst = 3;
//            template<class T> Mytemplate { ... }
//              // ONLY if Mytemplate is not used as the type of a parameter
//              // or return value of any exposed public OIIO API function!
//              // If it ever is, it should be moved to a versioned namespace.
//        OIIO_NAMESPACE_END
//
//    Because of the inline inner default namespace, user code can just refer
//    to these as `OIIO::myfunc()`, `OIIO::myconst`, etc.
//
// 2. Declarations carried over from previous versions and aliased into
//    later/current namespaces:
//
//    * Anything that was introduced in earlier versions and is enclosed in a
//      namespace (like `Filesystem`, `Strutil`, or `ImageBufAlgo`).
//    * Existing classes that were introduced in earlier OIIO versions, and
//      any modifications that can be amended without breaking their ABIs
//      (such as adding a non-virtual method). This also includes templated
//      classes used to pass parameters or return values by OIIO's APIs.
//    * Functions that were introduced in earlier versions and can simply
//      be aliased into subsequent namespaces with `using`.
//    * NEW items in the above categories that are logically related to
//      existing things should be placed in the oldest namespace where the
//      rest of its cohort lives, if that can be done without conflicting with
//      existing symbols.
//
//    These live in the namespace of the version where they were first
//    introduced. Later versions alias these into their namespaces with
//    `using`.
//
//        OIIO_NAMESPACE_3_1_BEGIN
//            class Myclass { ... };
//            template<class T> Mytemplate { ... }
//            int standalone_func();
//            namespace Group {
//                int func_in_group();
//            }
//        OIIO_NAMESPACE_3_1_END
//
//        // Alias these items into subsequent version namespaces
//        OIIO_NAMESPACE_BEGIN
//            using v3_1::Myclass;
//            using v3_1::Mytemplate;
//            using v3_1::standalone_func;
//            namespace Group {
//                using namespace v3_1::Group;
//                // Makes EVERYTHING declared in v3_1::Group alias to
//                // within the current namespace's Group.
//            }
//        OIIO_NAMESPACE_END
//
// 3. Declarations that must be separately defined in each namespace.
//
//    * Classes or functions whose declarations in newer versions have changed
//      in an ABI-incompatible way.
//    * Standalone functions or globals that are not enclosed in a namespace
//      and for whatever reason can't simply be pulled into later namespaces
//      with the `using` directive.
//
//    These must have separate symbols in each versioned namespace to preserve
//    ABI compatibility. But it is customary to make the full and most
//    efficient implementation in the newer namespace, and the others be
//    trivial wrappers (with some small penalty for the double call, but
//    that's ok because it's only incurred when linking against a too-new
//    version).
//
//    Declarations in the .h header:
//
//        OIIO_NAMESPACE_3_1_BEGIN
//            int myfunc();
//            class Myclass { int foo; ... }
//            int anotherfunc(Myclass& m);
//        OIIO_NAMESPACE_3_1_END
//
//        // Duplicate in subsequent version namespaces
//        OIIO_NAMESPACE_BEGIN
//            float myfunc();                   // changed return value
//            class Myclass { float foo; ... }  // data layout changed
//            int anotherfunc(Myclass& m);      // DIFFERENT Myclass!
//        OIIO_NAMESPACE_END
//
//    Implementation in the .cpp file:
//
//        OIIO_NAMESPACE_BEGIN
//            // Current namespace gets a full implementation of myfunc:
//            float myfunc() { ... }
//
//            // New anotherfunc takes new definition of Myclass:
//            int anotherfunc(Myclass& m) { ... }
//        OIIO_NAMESPACE_END
//
//        OIIO_NAMESPACE_3_1_BEGIN
//            // Old version
//            int myfunc() { ... }
//
//            // Old anotherfunc takes old definition of Myclass:
//            int anotherfunc(v3_1::Myclass& m) { ... }
//        OIIO_NAMESPACE_3_1_END
//


// Macros for defining namespaces with an explicit version
#define OIIO_NS_BEGIN(ver) namespace OIIO_OUTER_NAMESPACE { namespace ver {
#define OIIO_NS_END } }

// Specialty macro: Make something ABI compatible with 3.1
#define OIIO_NAMESPACE_3_1_BEGIN OIIO_NAMESPACE_BEGIN
#define OIIO_NAMESPACE_3_1_END OIIO_NAMESPACE_END




// Forward declarations of important classes
OIIO_NAMESPACE_3_1_BEGIN
// libOpenImageIO_Util
class ArgParse;
class ColorConfig;
class ColorProcessor;
class ErrorHandler;
class Filter1D;
class Filter2D;
class FilterDesc;
class ParamValue;
class ParamValueList;
class ParamValueSpan;
class ScopedTimer;
class Timer;
struct TypeDesc;
class ustring;
class ustringhash;

// libOpenImageIO
class DeepData;
class ImageBuf;
class ImageBufImpl;
class ImageCache;
class ImageCachePerThreadInfo;
class ImageCacheFile;
class ImageCacheImpl;
class ImageCacheTile;
class ImageInput;
class ImageOutput;
class ImageSpec;
class paropt;
struct ROI;
class TextureOptBatch_v1;
class TextureOpt_v2;
using TextureOpt = TextureOpt_v2;
class TextureSystem;
class TextureSystemImpl;
namespace Filesystem {
    class IOProxy;
}
namespace ImageBufAlgo { }
namespace Strutil { }
namespace Sysutil { }
namespace simd { }
OIIO_NAMESPACE_3_1_END

#endif /* defined(OPENIMAGEIO_NSVERSIONS_H) */
