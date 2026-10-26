// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <type_traits>

#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>


// Helper: print entire vectors. This makes the OIIO_CHECK_EQUAL macro
// work for std::vector.
template<typename T>
inline std::ostream&
operator<<(std::ostream& out, const std::vector<T>& v)
{
    out << '{' << OIIO::Strutil::join(v, ",") << '}';
    return out;
}


OIIO_NAMESPACE_BEGIN

namespace simd {
// Force a float-based abs and max to appear in namespace simd
inline float
abs(float x)
{
    return std::abs(x);
}
inline float
max(float x, float y)
{
    return std::max(x, y);
}
}  // namespace simd

namespace pvt {

class UnitTestFailureCounter {
public:
    UnitTestFailureCounter() noexcept : m_failures(0) {}
    ~UnitTestFailureCounter()
    {
        if (m_failures) {
            std::cout << Sysutil::Term(std::cout).ansi("red", "ERRORS!\n");
            std::exit(m_failures != 0);
        } else {
#ifndef OIIO_UNIT_TEST_QUIET_SUCCESS
            std::cout << Sysutil::Term(std::cout).ansi("green", "OK\n");
#endif
        }
    }
    const UnitTestFailureCounter& operator++() noexcept  // prefix
    {
        ++m_failures;
        return *this;
    }
    int operator++(int) noexcept { return m_failures++; }  // postfix
    UnitTestFailureCounter operator+=(int i) noexcept
    {
        m_failures += i;
        return *this;
    }
    operator int() const noexcept { return m_failures; }

private:
    int m_failures = 0;
};


template<typename X, typename Y>
inline bool
equal_approx(const X& x, const Y& y)
{
    using namespace simd;
    return all(abs((x) - (y)) <= 0.001f * max(abs(x), abs(y)));
}


// Helpers for the OIIO_CHECK_* comparison macros. Taking the values as
// function arguments evaluates each macro argument only once, and keeping the
// failure reports out of line keeps them from bloating the caller. Both
// matter for SIMD tests, where each argument may be a large expanded
// expression and the callers become very big functions that are slow to
// compile.

enum class ReportDiff { None, Diff, AbsDiff };

template<ReportDiff diff, typename X, typename Y>
OIIO_NOINLINE void
report_compare_failure(const char* file, int line, const char* xstr,
                       const char* op, const char* ystr, const X& x, const Y& y)
{
    using ::operator<<;  // for the std::vector printer above
    std::cout << Sysutil::Term(std::cout).ansi("red,bold") << file << ":"
              << line << ":\n"
              << "FAILED: " << Sysutil::Term(std::cout).ansi("normal") << xstr
              << " " << op << " " << ystr << "\n"
              << "\tvalues were '" << x << "' and '" << y << "'";
    if constexpr (diff == ReportDiff::Diff)
        std::cout << ", diff was " << (x - y);
    else if constexpr (diff == ReportDiff::AbsDiff)
        std::cout << ", diff was " << std::abs(x - y);
    std::cout << "\n";
}

// Cmp is a std comparison functor such as std::equal_to<>.
template<typename Cmp, typename X, typename Y>
inline bool
check_compare_impl(const X& x, const Y& y, const char* xstr, const char* op,
                   const char* ystr, const char* file, int line)
{
    if (Cmp()(x, y))
        return true;
    report_compare_failure<ReportDiff::None>(file, line, xstr, op, ystr, x, y);
    return false;
}

// Scalars are taken by value so that passing a static const data member that
// has no out-of-line definition does not odr-use it (link error).
template<typename Cmp, typename X, typename Y,
         OIIO_ENABLE_IF(std::is_scalar_v<X>&& std::is_scalar_v<Y>)>
inline bool
check_compare(X x, Y y, const char* xstr, const char* op, const char* ystr,
              const char* file, int line)
{
    return check_compare_impl<Cmp>(x, y, xstr, op, ystr, file, line);
}

template<typename Cmp, typename X, typename Y,
         OIIO_ENABLE_IF(!(std::is_scalar_v<std::decay_t<X>>
                          && std::is_scalar_v<std::decay_t<Y>>))>
inline bool
check_compare(const X& x, const Y& y, const char* xstr, const char* op,
              const char* ystr, const char* file, int line)
{
    return check_compare_impl<Cmp>(x, y, xstr, op, ystr, file, line);
}

template<typename X, typename Y, typename E>
inline bool
check_equal_thresh(const X& x, const Y& y, const E& eps, const char* xstr,
                   const char* ystr, const char* file, int line)
{
    if (std::abs(x - y) <= eps)
        return true;
    report_compare_failure<ReportDiff::AbsDiff>(file, line, xstr, "==", ystr, x,
                                                y);
    return false;
}

template<typename X, typename Y, typename EA, typename ER>
inline bool
check_equal_thresh_rel(const X& x, const Y& y, const EA& epsabs,
                       const ER& epsrel, const char* xstr, const char* ystr,
                       const char* file, int line)
{
    using std::abs;
    if (std::abs(x - y) <= (epsabs + epsrel * std::max(abs(x), abs(y))))
        return true;
    report_compare_failure<ReportDiff::AbsDiff>(file, line, xstr, "==", ystr, x,
                                                y);
    return false;
}

template<typename X, typename Y>
inline bool
check_equal_approx(const X& x, const Y& y, const char* xstr, const char* ystr,
                   const char* file, int line)
{
    if (equal_approx(x, y))
        return true;
    report_compare_failure<ReportDiff::Diff>(file, line, xstr, "==", ystr, x,
                                             y);
    return false;
}

template<typename X, typename Y>
inline bool
check_simd_equal(const X& x, const Y& y, const char* xstr, const char* ystr,
                 const char* file, int line)
{
    using namespace simd;
    if (all(x == y))
        return true;
    report_compare_failure<ReportDiff::None>(file, line, xstr, "==", ystr, x,
                                             y);
    return false;
}

template<typename X, typename Y>
OIIO_NOINLINE void
report_simd_thresh_failure(const char* file, int line, const char* xstr,
                           const char* ystr, const X& x, const Y& y)
{
    using namespace simd;
    Strutil::print("{}{}:{}:\nFAILED: {}{} == {}\n",
                   Sysutil::Term(std::cout).ansi("red,bold"), file, line,
                   Sysutil::Term(std::cout).ansi("normal"), xstr, ystr);
    Strutil::print("\tvalues were '{:.8g}' and '{:.8g}', diff was {:.8g}\n", x,
                   y, abs(x - y));
}

template<typename X, typename Y, typename E>
inline bool
check_simd_equal_thresh(const X& x, const Y& y, const E& eps, const char* xstr,
                        const char* ystr, const char* file, int line)
{
    using namespace simd;
    if (all(abs(x - y) <= eps))
        return true;
    report_simd_thresh_failure(file, line, xstr, ystr, x, y);
    return false;
}


}  // end namespace pvt

OIIO_NAMESPACE_END

static OIIO::pvt::UnitTestFailureCounter unit_test_failures;



/// OIIO_CHECK_* macros checks if the conditions is met, and if not,
/// prints an error message indicating the module and line where the
/// error occurred, but does NOT abort.  This is helpful for unit tests
/// where we do not want one failure.
#define OIIO_CHECK_ASSERT(x)                                                  \
    ((x) ? ((void)0)                                                          \
         : ((std::cout << OIIO::Sysutil::Term(std::cout).ansi("red,bold")     \
                       << __FILE__ << ":" << __LINE__ << ":\n"                \
                       << "FAILED: "                                          \
                       << OIIO::Sysutil::Term(std::cout).ansi("normal") << #x \
                       << "\n"),                                              \
            (void)++unit_test_failures))

#define OIIO_CHECK_EQUAL(x, y)                                         \
    (OIIO::pvt::check_compare<std::equal_to<>>((x), (y), #x, "==", #y, \
                                               __FILE__, __LINE__)     \
         ? ((void)0)                                                   \
         : (void)++unit_test_failures)

#define OIIO_CHECK_FALSE(x) OIIO_CHECK_EQUAL(x, false)

#define OIIO_CHECK_EQUAL_THRESH(x, y, eps)                                      \
    (OIIO::pvt::check_equal_thresh((x), (y), (eps), #x, #y, __FILE__, __LINE__) \
         ? ((void)0)                                                            \
         : (void)++unit_test_failures)

#define OIIO_CHECK_EQUAL_THRESH_REL(x, y, epsabs, epsrel)                    \
    (OIIO::pvt::check_equal_thresh_rel((x), (y), (epsabs), (epsrel), #x, #y, \
                                       __FILE__, __LINE__)                   \
         ? ((void)0)                                                         \
         : (void)++unit_test_failures)

#define OIIO_CHECK_EQUAL_APPROX(x, y)                                    \
    (OIIO::pvt::check_equal_approx((x), (y), #x, #y, __FILE__, __LINE__) \
         ? ((void)0)                                                     \
         : (void)++unit_test_failures)

#define OIIO_CHECK_NE(x, y)                                                \
    (OIIO::pvt::check_compare<std::not_equal_to<>>((x), (y), #x, "!=", #y, \
                                                   __FILE__, __LINE__)     \
         ? ((void)0)                                                       \
         : (void)++unit_test_failures)

#define OIIO_CHECK_LT(x, y)                                                 \
    (OIIO::pvt::check_compare<std::less<>>((x), (y), #x, "<", #y, __FILE__, \
                                           __LINE__)                        \
         ? ((void)0)                                                        \
         : (void)++unit_test_failures)

#define OIIO_CHECK_GT(x, y)                                                    \
    (OIIO::pvt::check_compare<std::greater<>>((x), (y), #x, ">", #y, __FILE__, \
                                              __LINE__)                        \
         ? ((void)0)                                                           \
         : (void)++unit_test_failures)

#define OIIO_CHECK_LE(x, y)                                              \
    (OIIO::pvt::check_compare<std::less_equal<>>((x), (y), #x, "<=", #y, \
                                                 __FILE__, __LINE__)     \
         ? ((void)0)                                                     \
         : (void)++unit_test_failures)

#define OIIO_CHECK_GE(x, y)                                                 \
    (OIIO::pvt::check_compare<std::greater_equal<>>((x), (y), #x, ">=", #y, \
                                                    __FILE__, __LINE__)     \
         ? ((void)0)                                                        \
         : (void)++unit_test_failures)


// Special SIMD related equality checks that use all()
#define OIIO_CHECK_SIMD_EQUAL(x, y)                                    \
    (OIIO::pvt::check_simd_equal((x), (y), #x, #y, __FILE__, __LINE__) \
         ? ((void)0)                                                   \
         : (void)++unit_test_failures)

#define OIIO_CHECK_SIMD_EQUAL_THRESH(x, y, eps)                            \
    (OIIO::pvt::check_simd_equal_thresh((x), (y), (eps), #x, #y, __FILE__, \
                                        __LINE__)                          \
         ? ((void)0)                                                       \
         : (void)++unit_test_failures)


// Test if ImageBuf operation got an error. It's a lot like simply testing
// OIIO_CHECK_ASSERT(x), but if x is false, it will get an error message
// from the buffer and incorporate it into the failure message.
// Call like this, for example:
//    ImageBuf buf;
//    OIIO_CHECK_IMAGEBUF_STATUS(buf,
//        ImageBufAlgo::Func (buf, ...)
//    );
#define OIIO_CHECK_IMAGEBUF_STATUS(buf, x)                                    \
    ((x && !buf.has_error())                                                  \
         ? ((void)0)                                                          \
         : ((std::cout << OIIO::Sysutil::Term(std::cout).ansi("red,bold")     \
                       << __FILE__ << ":" << __LINE__ << ":\n"                \
                       << "FAILED: "                                          \
                       << OIIO::Sysutil::Term(std::cout).ansi("normal") << #x \
                       << ": " << buf.geterror() << "\n"),                    \
            (void)++unit_test_failures))
