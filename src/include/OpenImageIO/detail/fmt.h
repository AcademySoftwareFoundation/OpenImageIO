// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#pragma once
#define OIIO_FMT_H

#include <OpenImageIO/platform.h>
#include <OpenImageIO/type_traits.h>

// We want the header-only implemention of fmt
#ifndef FMT_HEADER_ONLY
#    define FMT_HEADER_ONLY
#endif

// Disable fmt exceptions
#ifndef FMT_EXCEPTIONS
#    define FMT_EXCEPTIONS 0
#endif

// Use the grisu fast floating point formatting for old fmt versions
// (irrelevant for >= 7.1).
#ifndef FMT_USE_GRISU
#    define FMT_USE_GRISU 1
#endif

// fmt 8.1 stopped automatically enabling formatting of anything that supports
// ostream output. This breaks a lot! Re-enable this old behavior.
#ifndef FMT_DEPRECATED_OSTREAM
#    define FMT_DEPRECATED_OSTREAM 1
#endif

OIIO_PRAGMA_WARNING_PUSH
#if OIIO_GNUC_VERSION >= 70000
#    pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#if OIIO_INTEL_LLVM_COMPILER
#    pragma GCC diagnostic ignored "-Wtautological-constant-compare"
#endif

#include <OpenImageIO/detail/fmt/format.h>
#include <OpenImageIO/detail/fmt/ostream.h>
#include <OpenImageIO/detail/fmt/printf.h>

OIIO_PRAGMA_WARNING_POP


OIIO_NAMESPACE_BEGIN
namespace pvt {


// Custom inheritable parse() method used by fmt formatters. In addition to
// saving the formatting spec, it also checks for a (nonstandard) optional
// leading ',' and records it as a separator flag.
struct format_parser_with_separator {
    FMT_CONSTEXPR auto parse(fmt::format_parse_context& ctx)
    {
        auto beg = ctx.begin(), end = ctx.end();
        if (beg != end && *beg == ',')
            sep = *beg++;
        auto it = beg;  // where's the close brace?
        for (; it != end && *it != '}'; ++it)
            ;
        elem_fmt = fmt::string_view(beg, it - beg);
        return it;
    }

protected:
    fmt::string_view elem_fmt;
    char sep = 0;
};



// fmtlib custom formatter that formats a type `T` that has array-like
// semantics (must have a valid operator[] and size() method) by printing each
// element according to the format spec. For example, if the object has 3
// float elements and the spec is "{.3f}", then the output might be "1.234
// 2.345 3.456".
//
// In addition to the usual formatting spec, we also recognize the following
// special extension: If the first character of the format spec is ','
// (comma), then the array elements will be separated by ", " rather than the
// default " ".
//
// For example, `format("[{:,.3f}]")` will format the array as `[1.234, 2.345,
// 3.456]`.
//
// Now then, the way this class is helpful is to inherit from it for a custom
// formatter. For example, this will make this formatter be used for
// Imath::V3f:
//
//     // in global namespace:
//     template<> struct fmt::formatter<Imath::V3f>
//         : OIIO::array_formatter<Imath::V3f, float, 3> {};
//
template<typename T,
         OIIO_ENABLE_IF(has_subscript<T>::value&& has_size_method<T>::value)>
struct index_formatter : format_parser_with_separator {
    // inherits parse() from format_parser_with_separator
    template<typename FormatContext> auto format(const T& v, FormatContext& ctx)
    {
        std::string vspec = elem_fmt.size() ? fmt::format("{{:{}}}", elem_fmt)
                                            : std::string("{}");
        for (size_t i = 0; i < v.size(); ++i) {
            if (i)
                fmt::format_to(ctx.out(), "{}", sep == ',' ? ", " : " ");
#if FMT_VERSION >= 80000
            fmt::format_to(ctx.out(), fmt::runtime(vspec), v[i]);
#else
            fmt::format_to(ctx.out(), vspec, v[i]);
#endif
        }
        return ctx.out();
    }
};



// fmtlib custom formatter that formats a type `T` as if it were an array
// `Elem[Size]` (and it must be laid out that way in memory). The formatting
// spec will apply to each element. For example, if the object has 3 float
// elements and the spec is "{.3f}", then the output might be "1.234 2.345
// 3.456".
//
// In addition to the usual formatting spec, we also recognize the following
// special extension: If the first character of the format spec is ','
// (comma), then the array elements will be separated by ", " rather than the
// default " ".
//
// For example, `format("[{:,.3f}]")` will format the array as `[1.234, 2.345,
// 3.456]`.
//
// Now then, the way this class is helpful is to inherit from it for a custom
// formatter. For example, this will make this formatter be used for
// Imath::V3f:
//
//     // in global namespace:
//     template<> struct fmt::formatter<Imath::V3f>
//         : OIIO::array_formatter<Imath::V3f, float, 3> {};
//
template<typename T, typename Elem, int Size>
struct array_formatter : format_parser_with_separator {
    // inherits parse() from format_parser_with_separator
    template<typename FormatContext> auto format(const T& v, FormatContext& ctx)
    {
        std::string vspec = elem_fmt.size() ? fmt::format("{{:{}}}", elem_fmt)
                                            : std::string("{}");
        for (int i = 0; i < Size; ++i) {
            if (i)
                fmt::format_to(ctx.out(), "{}", sep == ',' ? ", " : " ");
#if FMT_VERSION >= 80000
            fmt::format_to(ctx.out(), fmt::runtime(vspec),
                           ((const Elem*)&v)[i]);
#else
            fmt::format_to(ctx.out(), vspec, ((const Elem*)&v)[i]);
#endif
        }
        return ctx.out();
    }
};


}  // namespace pvt
OIIO_NAMESPACE_END
