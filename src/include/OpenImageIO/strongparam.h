// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#pragma once

#include <type_traits>

#include <OpenImageIO/oiioversion.h>

OIIO_NAMESPACE_BEGIN


/// StrongParam is used to construct an implementation of a derived type
/// that lets you pass strongly typed parameters. It implicitly converts TO
/// the basetype, but requires explicit conversion FROM the basetype.
///
/// The problem this is meant to address is that you may have a function
/// that has multiple bool, int, or float parameters, particularly if they
/// are adjacent in the call signature. This is extremely error prone. For
/// example, suppose you have
///
///     void func (bool verbose, bool crazy, int apples, int oranges);
///
/// and then it is called:
///
///     func(true, false, 3, 8);
///
/// Is this correct, or does it harbor a bug? Your guess is as good as mine.
/// In comparison, Python has a syntax that lets you name parameters, which
/// looks like this:
///
///     func(verbose=true, crazy=false, apples=3, oranges=8);
///
/// But, unfortunately, no such syntax exists in C++. Maybe someday it will,
/// but for now, we want something we can use to make the function call
/// similarly clear. Like this:
///
///     func(Verbose(true), Crazy(false), Apples(3), Oranges(8));
///
/// and simultaneously for the following to be considered errors:
///
///     // Not allowed: bare bools and ints
///     func(true, false, 3, 8);
///
///     // Not allowed: getting the order wrong
///     func(Crazy(false), Verbose(true), Oranges(8), Apples(3));
///
/// Our solution is inspired by
/// https://lists.llvm.org/pipermail/llvm-dev/2019-August/134302.html
/// though we have simplified it quite a bit for our needs.
///
/// Example use 1: Use StrongParam to disambiguate parameters.
///
///     // Use macro to generate the new types
///     OIIO_STRONG_PARAM_TYPE(Verbose, bool);
///     OIIO_STRONG_PARAM_TYPE(Crazy, bool);
///
///     bool
///     compute (Verbose a, Crazy b)
///     {
///         return a | b;
///     }
///
///
/// Example 2: Use StrongParam to disambiguate two floats, a poor person's
/// implementation of units:
///
/// Error prone: speed(float,float)  // which is first, meters or seconds?
/// Unambiguous: speed(Meters,Seconds)
///
///     OIIO_STRONG_PARAM_TYPE(Meters, float);
///     OIIO_STRONG_PARAM_TYPE(Seconds, float);
///
///     float
///     speed (Meters a, Seconds b)
///     {
///         return a / b;
///     }
///
/// Note that the fancy strong type is for declaration purposes. Any time
/// you use it in the function, it implicitly converts to the underlying
/// base type.
///
/// As an alternative to `OIIO_STRONG_TYPE(Meters, float)`, you may also use
/// this notation (if you find it more pleasing):
///
///     using Meters = StrongParam<struct MetersTag, float>;
///
/// The MetersTag struct need not be defined anywhere, it just needs to
/// be a unique name.
///

template<typename Tag, typename Basetype> struct StrongParam {
    // Construct a StrongParam from a Basetype.
    explicit StrongParam(const Basetype& val)
        : m_val(val)
    {
    }

    // Allow default simple copy construction
    StrongParam(const StrongParam<Tag, Basetype>& val) = default;

    // Allow implicit conversion back to Basetype.
    operator const Basetype&() const noexcept { return m_val; }

private:
    Basetype m_val;
    static_assert(std::is_trivial<Basetype>::value, "Need trivial type");
};



/// Convenience macro for making strong parameter type Name that is Basetype
/// underneath. What it actually does is make a new type that is derived
/// from StrongParam<Name,Basetype>.
#define OIIO_STRONG_PARAM_TYPE(Name, Basetype)         \
    struct Name : public StrongParam<Name, Basetype> { \
        using StrongParam::StrongParam;                \
    }


OIIO_NAMESPACE_END
