// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO
// clang-format off

#include <cstdio>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>

using namespace OIIO;



void
test_format()
{
    std::cout << "testing format()/sprintf()" << std::endl;

    // Test formatting with Strutil::sprintf()
    // ---------------------------------------
    OIIO_CHECK_EQUAL(Strutil::sprintf("%d %f %g", int(3), 3.14f, 3.14f),
                     "3 3.140000 3.14");
    OIIO_CHECK_EQUAL(Strutil::sprintf("'%s' '%s'", "foo", std::string("foo")),
                     "'foo' 'foo'");
    OIIO_CHECK_EQUAL(Strutil::sprintf("'%3d' '%03d' '%-3d'", 3, 3, 3),
                     "'  3' '003' '3  '");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%+d%+d%+d", 3, -3, 0), "+3-3+0");
    OIIO_CHECK_EQUAL(Strutil::sprintf("foo"), "foo");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%%foo"), "%foo");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%d", int16_t(0xffff)), "-1");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%u", uint16_t(0xffff)), "65535");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%d", int32_t(0xffffffff)), "-1");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%u", uint32_t(0xffffffff)), "4294967295");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%d", int64_t(0xffffffffffffffff)), "-1");
    OIIO_CHECK_EQUAL(Strutil::sprintf("%u", uint64_t(0xffffffffffffffff)), "18446744073709551615");

    // Test formatting with Strutil::fmt::format(), which uses the
    // Python conventions:
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{} {:f} {}", int(3), 3.14f, 3.14f),
                     "3 3.140000 3.14");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("'{}' '{}'", "foo", std::string("foo")),
                     "'foo' 'foo'");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("'{:3d}' '{:03d}' '{:<3d}'", 3, 3, 3),
                     "'  3' '003' '3  '");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{:+d}{:+d}{:+d}", 3, -3, 0), "+3-3+0");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("foo"), "foo");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("%foo"), "%foo");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", short(0xffff)), "-1");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", uint16_t(0xffff)), "65535");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", int32_t(0xffffffff)), "-1");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", uint32_t(0xffffffff)), "4294967295");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", int64_t(0xffffffffffffffff)), "-1");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", uint64_t(0xffffffffffffffff)), "18446744073709551615");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{} {:f} {:g}", int(3), 3.14f, 3.14f),
                     "3 3.140000 3.14");

    Benchmarker bench;
    bench.indent (2);
    bench.units (Benchmarker::Unit::ns);
    char buffer[256];
    bench ("std::snprintf(\"%g\")", [&](){ DoNotOptimize (std::snprintf(buffer,sizeof(buffer),"%g",123.45f)); });
    bench ("Strutil::sprintf(\"%g\")", [&](){ DoNotOptimize (Strutil::sprintf("%g",123.45f)); });
    bench ("Strutil::fmt::format(\"{:g}\")", [&](){ DoNotOptimize (Strutil::fmt::format("{:g}",123.45f)); });
    bench ("Strutil::to_string(float)", [&](){ DoNotOptimize (Strutil::to_string(123.45f)); });

    bench ("std::snprintf(\"%d\")", [&](){ DoNotOptimize (std::snprintf(buffer,sizeof(buffer),"%d",123)); });
    bench ("Strutil::sprintf(\"%d\")", [&](){ DoNotOptimize (Strutil::sprintf("%g",123.0f)); });
    bench ("Strutil::fmt::format(\"{}\")", [&](){ DoNotOptimize (Strutil::fmt::format("{}",123)); });
    bench ("Strutil::to_string(int)", [&](){ DoNotOptimize (Strutil::to_string(123)); });

    bench ("std::snprintf(\"%g %d %s %d %s %g\")", [&](){
               DoNotOptimize (std::snprintf(buffer,sizeof(buffer),"%g %d %s %d %s %g", 123.45f, 1234, "foobar", 42, "kablooey", 3.14159f));
           });
    bench ("Strutil::sprintf(\"%g %d %s %d %s %g\")", [&](){
               DoNotOptimize (Strutil::sprintf("%g %d %s %d %s %g", 123.45f, 1234, "foobar", 42, "kablooey", 3.14159f));
           });
    bench ("Strutil::fmt::format(\"{} {} {} {} {} {}\")", [&](){
               DoNotOptimize (Strutil::fmt::format("{} {} {} {} {} {}", 123.45f, 1234, "foobar", 42, "kablooey", 3.14159f));
           });
}



void
test_format_custom()
{
    std::cout << "testing format() custom formatters" << std::endl;

    simd::vfloat3 vf3iota = simd::vfloat3::Iota(1.5f);
    Strutil::print("vfloat3 {{}}  '{}'\n", vf3iota);
    Strutil::print("vfloat3 {{:.3f}}  '{:.3f}'\n", vf3iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vf3iota),
                     "X|1.5 2.5 3.5|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", vf3iota),
                     "X|1.500 2.500 3.500|Y");

    simd::vfloat4 vf4iota = simd::vfloat4::Iota(1.5f);
    Strutil::print("vfloat4 {{}}  '{}'\n", vf4iota);
    Strutil::print("vfloat4 {{:.3f}}  '{:.3f}'\n", vf4iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vf4iota),
                     "X|1.5 2.5 3.5 4.5|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", vf4iota),
                     "X|1.500 2.500 3.500 4.500|Y");

    simd::vfloat8 vf8iota = simd::vfloat8::Iota(1.5f);
    Strutil::print("vfloat8 {{}}  '{}'\n", vf8iota);
    Strutil::print("vfloat8 {{:.3f}}  '{:.3f}'\n", vf8iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vf8iota),
                     "X|1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", vf8iota),
                     "X|1.500 2.500 3.500 4.500 5.500 6.500 7.500 8.500|Y");

    simd::vfloat16 vf16iota = simd::vfloat16::Iota(1.5f);
    Strutil::print("vfloat16 {{}}  '{}'\n", vf16iota);
    Strutil::print("vfloat16 {{:.3f}}  '{:.3f}'\n", vf16iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vf16iota),
                     "X|1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5 10.5 11.5 12.5 13.5 14.5 15.5 16.5|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", vf16iota),
                     "X|1.500 2.500 3.500 4.500 5.500 6.500 7.500 8.500 9.500 10.500 11.500 12.500 13.500 14.500 15.500 16.500|Y");


    simd::vint4 vi4iota = simd::vint4::Iota(1);
    Strutil::print("vint4 {{}}  '{}'\n", vi4iota);
    Strutil::print("vint4 {{:03d}}  '{:03d}'\n", vi4iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vi4iota),
                     "X|1 2 3 4|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:03d}|Y", vi4iota),
                     "X|001 002 003 004|Y");

    simd::vint8 vi8iota = simd::vint8::Iota(1);
    Strutil::print("vint8 {{}}  '{}'\n", vi8iota);
    Strutil::print("vint8 {{:03d}}  '{:03d}'\n", vi8iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vi8iota),
                     "X|1 2 3 4 5 6 7 8|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:03d}|Y", vi8iota),
                     "X|001 002 003 004 005 006 007 008|Y");

    simd::vint16 vi16iota = simd::vint16::Iota(1);
    Strutil::print("vint16 {{}}  '{}'\n", vi16iota);
    Strutil::print("vint16 {{:03d}}  '{:03d}'\n", vi16iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", vi16iota),
                     "X|1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:03d}|Y", vi16iota),
                     "X|001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016|Y");

    simd::matrix44 m44iota(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    Strutil::print("matrix44 {{}}  '{}'\n", m44iota);
    Strutil::print("matrix44 {{:.3f}}  '{:.3f}'\n", m44iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", m44iota),
                     Strutil::fmt::format("{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}",
                                          m44iota[0][0], m44iota[0][1], m44iota[0][2], m44iota[0][3],
                                          m44iota[1][0], m44iota[1][1], m44iota[1][2], m44iota[1][3],
                                          m44iota[2][0], m44iota[2][1], m44iota[2][2], m44iota[2][3],
                                          m44iota[3][0], m44iota[3][1], m44iota[3][2], m44iota[3][3]));
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", m44iota),
                     "X|0.000 1.000 2.000 3.000 4.000 5.000 6.000 7.000 8.000 9.000 10.000 11.000 12.000 13.000 14.000 15.000|Y");

    Imath::V3f ivf3iota(1.5f, 2.5f, 3.5f);
    Strutil::print("Imath::V3f {{}}  '{}'\n", ivf3iota);
    Strutil::print("Imath::V3f {{:.3f}}  '{:.3f}'\n", ivf3iota);
    Strutil::print("Imath::V3f {{:,.3f}}  '{:,.3f}'\n", ivf3iota);
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", ivf3iota),
                     "X|1.5 2.5 3.5|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", ivf3iota),
                     "X|1.500 2.500 3.500|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|({:,.3f})|Y", ivf3iota),
                     "X|(1.500, 2.500, 3.500)|Y");
    Strutil::print("\n");

    // Test custom formatting of spans
    float farray[] = { 1.5f, 2.5f, 3.5f, 4.5f };
    Strutil::print("cspan<float> {{}}  '{}'\n", cspan<float>(farray));
    Strutil::print("cspan<float> {{:.3f}}  '{:.3f}'\n", cspan<float>(farray));
    Strutil::print("cspan<float> {{:,.3f}}  '{:,.3f}'\n", cspan<float>(farray));
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{}|Y", cspan<float>(farray)),
                     "X|1.5 2.5 3.5 4.5|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|{:.3f}|Y", cspan<float>(farray)),
                     "X|1.500 2.500 3.500 4.500|Y");
    OIIO_CHECK_EQUAL(Strutil::fmt::format("X|({:,.3f})|Y", cspan<float>(farray)),
                     "X|(1.500, 2.500, 3.500, 4.500)|Y");
}



void
test_memformat()
{
    OIIO_CHECK_EQUAL(Strutil::memformat(15), "15 B");
    OIIO_CHECK_EQUAL(Strutil::memformat(15LL * 1024), "15 KB");
    OIIO_CHECK_EQUAL(Strutil::memformat(15LL * 1024 * 1024), "15.0 MB");
    OIIO_CHECK_EQUAL(Strutil::memformat(15LL * 1024 * 1024 * 1024), "15.0 GB");
    OIIO_CHECK_EQUAL(Strutil::memformat(15LL * 1024 * 1024 + 200000),
                     "15.2 MB");
    OIIO_CHECK_EQUAL(Strutil::memformat(15LL * 1024 * 1024 + 200000, 3),
                     "15.191 MB");
}



void
test_timeintervalformat()
{
    OIIO_CHECK_EQUAL(Strutil::timeintervalformat(15.321), "15.3s");
    OIIO_CHECK_EQUAL(Strutil::timeintervalformat(150.321), "2m 30.3s");
    OIIO_CHECK_EQUAL(Strutil::timeintervalformat(15000.321), "4h 10m 0.3s");
    OIIO_CHECK_EQUAL(Strutil::timeintervalformat(150000.321),
                     "1d 17h 40m 0.3s");
    OIIO_CHECK_EQUAL(Strutil::timeintervalformat(150.321, 2), "2m 30.32s");
}



void
test_get_rest_arguments()
{
    bool ret;
    std::map<std::string, std::string> result;
    std::string base;
    std::string url = "someplace?arg1=value1&arg2=value2";
    ret             = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, true);
    OIIO_CHECK_EQUAL(base, "someplace");
    OIIO_CHECK_EQUAL(result["arg1"], "value1");
    OIIO_CHECK_EQUAL(result["arg2"], "value2");
    OIIO_CHECK_EQUAL(result["arg3"], "");

    result.clear();
    url = "?arg1=value1&arg2=value2";
    ret = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, true);
    OIIO_CHECK_EQUAL(base, "");
    OIIO_CHECK_EQUAL(result["arg1"], "value1");
    OIIO_CHECK_EQUAL(result["arg2"], "value2");

    result.clear();
    url = "arg1=value1&arg2=value2";
    ret = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, true);
    OIIO_CHECK_EQUAL(base, "arg1=value1&arg2=value2");
    OIIO_CHECK_EQUAL(result["arg1"], "");
    OIIO_CHECK_EQUAL(result["arg2"], "");

    result.clear();
    url = "";
    ret = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, true);
    OIIO_CHECK_EQUAL(base, "");
    OIIO_CHECK_EQUAL(result["arg1"], "");
    OIIO_CHECK_EQUAL(result["arg2"], "");

    result.clear();
    url = "sometextwithoutasense????&&&&&arg4=val1";
    ret = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, false);
    OIIO_CHECK_EQUAL(base, "sometextwithoutasense");
    OIIO_CHECK_EQUAL(result["arg1"], "");
    OIIO_CHECK_EQUAL(result["arg2"], "");
    OIIO_CHECK_EQUAL(result["arg4"], "");

    result.clear();
    url = "atext?arg1value1&arg2value2";
    ret = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, false);
    OIIO_CHECK_EQUAL(base, "atext");
    OIIO_CHECK_EQUAL(result["arg1"], "");
    OIIO_CHECK_EQUAL(result["arg2"], "");

    result.clear();
    url            = "atext?arg1=value1&arg2value2";
    result["arg2"] = "somevalue";
    ret            = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, false);
    OIIO_CHECK_EQUAL(base, "atext");
    OIIO_CHECK_EQUAL(result["arg1"], "value1");
    OIIO_CHECK_EQUAL(result["arg2"], "somevalue");

    // Test windows long filename syntax
    result.clear();
    url = "\\\\?\\UNC\\server\\foo?arg1=value1";
    ret = Strutil::get_rest_arguments(url, base, result);
    OIIO_CHECK_EQUAL(ret, true);
    OIIO_CHECK_EQUAL(base, "\\\\?\\UNC\\server\\foo");
    OIIO_CHECK_EQUAL(result["arg1"], "value1");
}



void
test_escape(string_view raw, string_view escaped)
{
    Strutil::print("escape '{}' <-> '{}'\n", raw, escaped);
    OIIO_CHECK_EQUAL(Strutil::escape_chars(raw), escaped);
    OIIO_CHECK_EQUAL(Strutil::unescape_chars(escaped), raw);
}



void
test_escape_sequences()
{
    test_escape ("\\ \n \r \t \v \b \f \a", "\\\\ \\n \\r \\t \\v \\b \\f \\a");
    test_escape (" \"quoted\" ",  " \\\"quoted\\\" ");
    OIIO_CHECK_EQUAL(Strutil::unescape_chars("A\\023B"), "A\023B");
}



void
test_wordwrap()
{
    std::string words
        = "Now is the time for all good men to come to the aid of their party.";
    OIIO_CHECK_EQUAL(Strutil::wordwrap(words, 24), "Now is the time for all\n"
                                                   "good men to come to the\n"
                                                   "aid of their party.");
    std::string densewords
        = "Now is the,time,for,all,good,men,to,come to the aid of their party.";
    OIIO_CHECK_EQUAL(Strutil::wordwrap(densewords, 24, 0, " ", ","),
                     "Now is the,time,for,all,\n"
                     "good,men,to,come to the\n"
                     "aid of their party.");
}



void
test_hash()
{
    using namespace Strutil;
    OIIO_CHECK_EQUAL(strhash("foo"), size_t(6150913649986995171));
    OIIO_CHECK_EQUAL(strhash(std::string("foo")), size_t(6150913649986995171));
    OIIO_CHECK_EQUAL(strhash(string_view("foo")), size_t(6150913649986995171));
    OIIO_CHECK_EQUAL(strhash(""), 0);  // empty string hashes to 0
    // Check longer hash and ensure that it's really constexpr
    constexpr size_t hash = Strutil::strhash("much longer string");
    OIIO_CHECK_EQUAL(hash, size_t(16257490369375554819ULL));
}



void
test_comparisons()
{
    OIIO_CHECK_EQUAL(Strutil::iequals("abc", "abc"), true);
    OIIO_CHECK_EQUAL(Strutil::iequals("Abc", "aBc"), true);
    OIIO_CHECK_EQUAL(Strutil::iequals("abc", "adc"), false);
    OIIO_CHECK_EQUAL(Strutil::iequals("abc", "abcd"), false);
    OIIO_CHECK_EQUAL(Strutil::iequals("abcd", "abc"), false);
    OIIO_CHECK_EQUAL(Strutil::iequals("", "abc"), false);
    OIIO_CHECK_EQUAL(Strutil::iequals("abc", ""), false);
    OIIO_CHECK_EQUAL(Strutil::iequals("", ""), true);

    OIIO_CHECK_EQUAL(Strutil::starts_with("abcd", "ab"), true);
    OIIO_CHECK_EQUAL(Strutil::starts_with("aBcd", "Ab"), false);
    OIIO_CHECK_EQUAL(Strutil::starts_with("abcd", "ba"), false);
    OIIO_CHECK_EQUAL(Strutil::starts_with("abcd", "abcde"), false);
    OIIO_CHECK_EQUAL(Strutil::starts_with("", "a"), false);
    OIIO_CHECK_EQUAL(Strutil::starts_with("", ""), true);
    OIIO_CHECK_EQUAL(Strutil::starts_with("abc", ""), true);

    OIIO_CHECK_EQUAL(Strutil::istarts_with("abcd", "ab"), true);
    OIIO_CHECK_EQUAL(Strutil::istarts_with("aBcd", "Ab"), true);
    OIIO_CHECK_EQUAL(Strutil::istarts_with("abcd", "ba"), false);
    OIIO_CHECK_EQUAL(Strutil::istarts_with("abcd", "abcde"), false);
    OIIO_CHECK_EQUAL(Strutil::istarts_with("", "a"), false);
    OIIO_CHECK_EQUAL(Strutil::istarts_with("", ""), true);
    OIIO_CHECK_EQUAL(Strutil::istarts_with("abc", ""), true);

    OIIO_CHECK_EQUAL(Strutil::ends_with("abcd", "cd"), true);
    OIIO_CHECK_EQUAL(Strutil::ends_with("aBCd", "cd"), false);
    OIIO_CHECK_EQUAL(Strutil::ends_with("aBcd", "CD"), false);
    OIIO_CHECK_EQUAL(Strutil::ends_with("abcd", "ba"), false);
    OIIO_CHECK_EQUAL(Strutil::ends_with("abcd", "xabcd"), false);
    OIIO_CHECK_EQUAL(Strutil::ends_with("", "a"), false);
    OIIO_CHECK_EQUAL(Strutil::ends_with("", ""), true);
    OIIO_CHECK_EQUAL(Strutil::ends_with("abc", ""), true);

    OIIO_CHECK_EQUAL(Strutil::iends_with("abcd", "cd"), true);
    OIIO_CHECK_EQUAL(Strutil::iends_with("aBCd", "cd"), true);
    OIIO_CHECK_EQUAL(Strutil::iends_with("aBcd", "CD"), true);
    OIIO_CHECK_EQUAL(Strutil::iends_with("abcd", "ba"), false);
    OIIO_CHECK_EQUAL(Strutil::iends_with("abcd", "xabcd"), false);
    OIIO_CHECK_EQUAL(Strutil::iends_with("", "a"), false);
    OIIO_CHECK_EQUAL(Strutil::iends_with("", ""), true);
    OIIO_CHECK_EQUAL(Strutil::iends_with("abc", ""), true);

    OIIO_CHECK_EQUAL(Strutil::contains("abcde", "ab"), true);
    OIIO_CHECK_EQUAL(Strutil::contains("abcde", "bcd"), true);
    OIIO_CHECK_EQUAL(Strutil::contains("abcde", "de"), true);
    OIIO_CHECK_EQUAL(Strutil::contains("abcde", "cdx"), false);
    OIIO_CHECK_EQUAL(Strutil::contains("abcde", ""), true);
    OIIO_CHECK_EQUAL(Strutil::contains("", ""), false);
    OIIO_CHECK_EQUAL(Strutil::contains("", "x"), false);

    OIIO_CHECK_EQUAL(Strutil::icontains("abcde", "ab"), true);
    OIIO_CHECK_EQUAL(Strutil::icontains("Abcde", "aB"), true);
    OIIO_CHECK_EQUAL(Strutil::icontains("abcde", "bcd"), true);
    OIIO_CHECK_EQUAL(Strutil::icontains("Abcde", "bCd"), true);
    OIIO_CHECK_EQUAL(Strutil::icontains("abcDe", "dE"), true);
    OIIO_CHECK_EQUAL(Strutil::icontains("abcde", "cdx"), false);
    OIIO_CHECK_EQUAL(Strutil::icontains("abcde", ""), true);
    OIIO_CHECK_EQUAL(Strutil::icontains("", ""), false);
    OIIO_CHECK_EQUAL(Strutil::icontains("", "x"), false);

    OIIO_CHECK_EQUAL(Strutil::rcontains("abcde", "ab"), true);
    OIIO_CHECK_EQUAL(Strutil::rcontains("abcde", "bcd"), true);
    OIIO_CHECK_EQUAL(Strutil::rcontains("abcde", "de"), true);
    OIIO_CHECK_EQUAL(Strutil::rcontains("abcde", "cdx"), false);
    OIIO_CHECK_EQUAL(Strutil::rcontains("abcde", ""), true);
    OIIO_CHECK_EQUAL(Strutil::rcontains("", ""), false);
    OIIO_CHECK_EQUAL(Strutil::rcontains("", "x"), false);

    OIIO_CHECK_EQUAL(Strutil::ircontains("abcde", "ab"), true);
    OIIO_CHECK_EQUAL(Strutil::ircontains("Abcde", "aB"), true);
    OIIO_CHECK_EQUAL(Strutil::ircontains("abcde", "bcd"), true);
    OIIO_CHECK_EQUAL(Strutil::ircontains("Abcde", "bCd"), true);
    OIIO_CHECK_EQUAL(Strutil::ircontains("abcDe", "dE"), true);
    OIIO_CHECK_EQUAL(Strutil::ircontains("abcde", "cdx"), false);
    OIIO_CHECK_EQUAL(Strutil::ircontains("abcde", ""), true);
    OIIO_CHECK_EQUAL(Strutil::ircontains("", ""), false);
    OIIO_CHECK_EQUAL(Strutil::ircontains("", "x"), false);

    OIIO_CHECK_EQUAL(Strutil::contains_any_char("abcde", "xa"), true);
    OIIO_CHECK_EQUAL(Strutil::contains_any_char("abcde", "xe"), true);
    OIIO_CHECK_EQUAL(Strutil::contains_any_char("abcde", "xc"), true);
    OIIO_CHECK_EQUAL(Strutil::contains_any_char("abcde", "xyz"), false);
    OIIO_CHECK_EQUAL(Strutil::contains_any_char("abcde", "abcde"), true);
    OIIO_CHECK_EQUAL(Strutil::contains_any_char("", "abc"), false);
    OIIO_CHECK_EQUAL(Strutil::contains_any_char("abcde", ""), false);

    OIIO_CHECK_EQUAL(Strutil::find("abcdeabcde", "bc"), 1);
    OIIO_CHECK_EQUAL(Strutil::find("abcdeabcde", "BC"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::find("abcdeabcde", "ac"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::find("abcdeabcde", ""), 0);
    OIIO_CHECK_EQUAL(Strutil::find("", "abc"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::find("", ""), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::rfind("abcdeabcde", "bc"), 6);
    OIIO_CHECK_EQUAL(Strutil::rfind("abcdeabcde", "BC"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::rfind("abcdeabcde", "ac"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::rfind("abcdeabcde", ""), 10);
    OIIO_CHECK_EQUAL(Strutil::rfind("", "abc"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::rfind("", ""), std::string::npos);

    OIIO_CHECK_EQUAL(Strutil::ifind("abcdeabcde", "bc"), 1);
    OIIO_CHECK_EQUAL(Strutil::ifind("abcdeabcde", "BC"), 1);
    OIIO_CHECK_EQUAL(Strutil::ifind("abcdeabcde", "ac"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::ifind("abcdeabcde", ""), 0);
    OIIO_CHECK_EQUAL(Strutil::ifind("Xabcdeabcde", "x"), 0);
    OIIO_CHECK_EQUAL(Strutil::ifind("abcdeabcdeX", "x"), 10);
    OIIO_CHECK_EQUAL(Strutil::ifind("", "abc"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::ifind("", ""), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::irfind("abcdeabcde", "bc"), 6);
    OIIO_CHECK_EQUAL(Strutil::irfind("abcdeabcde", "BC"), 6);
    OIIO_CHECK_EQUAL(Strutil::irfind("abcdeabcde", "ac"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::irfind("abcdeabcde", ""), 10);
    OIIO_CHECK_EQUAL(Strutil::irfind("Xabcdeabcde", "x"), 0);
    OIIO_CHECK_EQUAL(Strutil::irfind("abcdeabcdeX", "x"), 10);
    OIIO_CHECK_EQUAL(Strutil::irfind("", "abc"), std::string::npos);
    OIIO_CHECK_EQUAL(Strutil::irfind("", ""), std::string::npos);

    Strutil::StringEqual eq;
    Strutil::StringIEqual ieq;
    Strutil::StringLess less;
    Strutil::StringILess iless;
    OIIO_CHECK_ASSERT(eq("abc", "abc"));
    OIIO_CHECK_ASSERT(!eq("abc", "ABC"));
    OIIO_CHECK_ASSERT(!eq("abc", "axc"));
    OIIO_CHECK_ASSERT(ieq("abc", "abc"));
    OIIO_CHECK_ASSERT(ieq("abc", "ABC"));
    OIIO_CHECK_ASSERT(!ieq("abc", "axc"));
    OIIO_CHECK_ASSERT(less("abc", "abd"));
    OIIO_CHECK_ASSERT(!less("xbc", "abd"));
    OIIO_CHECK_ASSERT(!less("abc", "ABD"));
    OIIO_CHECK_ASSERT(iless("abc", "abd"));
    OIIO_CHECK_ASSERT(!iless("xbc", "abd"));
    OIIO_CHECK_ASSERT(iless("abc", "ABD"));

    Benchmarker bench;
    bench.indent (2);
    bench.units (Benchmarker::Unit::ns);
    std::string abc = "abcdefghijklmnopqrstuvwxyz";
    std::string abcmore = "abcdefghijklmnopqrstuvwxyz1";
    std::string abcnope = "1abcdefghijklmnopqrstuvwxyz";
    std::string haystack = std::string("begin") + abc + "oiio"
                         + Strutil::repeat(abc, 10) + "123" + abc + "end";
    bench ("string== success", [&](){ DoNotOptimize(abc == abc); }); // NOSONAR
    bench ("string== failure", [&](){ DoNotOptimize(abc == abcmore); });
    bench ("iequals success", [&](){ DoNotOptimize(Strutil::iequals(abc, abc)); });
    bench ("iless easy", [&](){ DoNotOptimize(Strutil::iless(abc, abcnope)); });
    bench ("iless hard", [&](){ DoNotOptimize(Strutil::iless(abc, abc)); });
    bench ("StringILess easy", [&](){ DoNotOptimize(iless(abc, abcnope)); });
    bench ("StringILess hard", [&](){ DoNotOptimize(iless(abc, abc)); });
    bench ("contains early small", [&](){ DoNotOptimize(Strutil::contains(abc, "def")); });
    bench ("contains early big", [&](){ DoNotOptimize(Strutil::contains(haystack, "oiio")); });
    bench ("contains late small", [&](){ DoNotOptimize(Strutil::contains(abc, "uvw")); });
    bench ("contains late big", [&](){ DoNotOptimize(Strutil::contains(haystack, "123")); });
    bench ("contains fail/small", [&](){ DoNotOptimize(Strutil::contains(abc, "dog")); });
    bench ("contains fail/big", [&](){ DoNotOptimize(Strutil::contains(haystack, "dog")); });
    bench ("rcontains early small", [&](){ DoNotOptimize(Strutil::rcontains(abc, "def")); });
    bench ("rcontains early big", [&](){ DoNotOptimize(Strutil::rcontains(haystack, "oiio")); });
    bench ("rcontains late small", [&](){ DoNotOptimize(Strutil::rcontains(abc, "uvw")); });
    bench ("rcontains late big", [&](){ DoNotOptimize(Strutil::rcontains(haystack, "123")); });
    bench ("rcontains fail/small", [&](){ DoNotOptimize(Strutil::rcontains(abc, "dog")); });
    bench ("rcontains fail/big", [&](){ DoNotOptimize(Strutil::rcontains(haystack, "dog")); });
    bench ("icontains early small", [&](){ DoNotOptimize(Strutil::icontains(abc, "def")); });
    bench ("icontains early big", [&](){ DoNotOptimize(Strutil::icontains(haystack, "oiio")); });
    bench ("icontains late small", [&](){ DoNotOptimize(Strutil::icontains(abc, "uvw")); });
    bench ("icontains late big", [&](){ DoNotOptimize(Strutil::icontains(haystack, "123")); });
    bench ("icontains fail/small", [&](){ DoNotOptimize(Strutil::icontains(abc, "dog")); });
    bench ("icontains fail/big", [&](){ DoNotOptimize(Strutil::icontains(haystack, "dog")); });

    bench ("find early small", [&](){ DoNotOptimize(Strutil::find(abc, "def")); });
    bench ("find early big", [&](){ DoNotOptimize(Strutil::find(haystack, "oiio")); });
    bench ("find late small", [&](){ DoNotOptimize(Strutil::find(abc, "uvw")); });
    bench ("find late big", [&](){ DoNotOptimize(Strutil::find(haystack, "123")); });
    bench ("find fail/small", [&](){ DoNotOptimize(Strutil::find(abc, "dog")); });
    bench ("find fail/big", [&](){ DoNotOptimize(Strutil::find(haystack, "dog")); });
    bench ("rfind early small", [&](){ DoNotOptimize(Strutil::rfind(abc, "def")); });
    bench ("rfind early big", [&](){ DoNotOptimize(Strutil::rfind(haystack, "oiio")); });
    bench ("rfind late small", [&](){ DoNotOptimize(Strutil::rfind(abc, "uvw")); });
    bench ("rfind late big", [&](){ DoNotOptimize(Strutil::rfind(haystack, "123")); });
    bench ("rfind fail/small", [&](){ DoNotOptimize(Strutil::rfind(abc, "dog")); });
    bench ("rfind fail/big", [&](){ DoNotOptimize(Strutil::rfind(haystack, "dog")); });

    bench ("ifind early small", [&](){ DoNotOptimize(Strutil::ifind(abc, "def")); });
    bench ("ifind early big", [&](){ DoNotOptimize(Strutil::ifind(haystack, "oiio")); });
    bench ("ifind late small", [&](){ DoNotOptimize(Strutil::ifind(abc, "uvw")); });
    bench ("ifind late big", [&](){ DoNotOptimize(Strutil::ifind(haystack, "123")); });
    bench ("ifind fail/small", [&](){ DoNotOptimize(Strutil::ifind(abc, "dog")); });
    bench ("ifind fail/big", [&](){ DoNotOptimize(Strutil::ifind(haystack, "dog")); });
    bench ("irfind early small", [&](){ DoNotOptimize(Strutil::irfind(abc, "def")); });
    bench ("irfind early big", [&](){ DoNotOptimize(Strutil::irfind(haystack, "oiio")); });
    bench ("irfind late small", [&](){ DoNotOptimize(Strutil::irfind(abc, "uvw")); });
    bench ("irfind late big", [&](){ DoNotOptimize(Strutil::irfind(haystack, "123")); });
    bench ("irfind fail/small", [&](){ DoNotOptimize(Strutil::irfind(abc, "dog")); });
    bench ("irfind fail/big", [&](){ DoNotOptimize(Strutil::irfind(haystack, "dog")); });

    bench ("starts_with success", [&](){ DoNotOptimize(Strutil::starts_with(abc, "abc")); });
    bench ("starts_with fail", [&](){ DoNotOptimize(Strutil::starts_with(abc, "def")); });
    bench ("istarts_with success", [&](){ DoNotOptimize(Strutil::istarts_with(abc, "abc")); });
    bench ("istarts_with fail", [&](){ DoNotOptimize(Strutil::istarts_with(abc, "def")); });
    bench ("ends_with success", [&](){ DoNotOptimize(Strutil::ends_with(abc, "xyz")); });
    bench ("ends_with fail", [&](){ DoNotOptimize(Strutil::ends_with(abc, "def")); });
    bench ("iends_with success", [&](){ DoNotOptimize(Strutil::iends_with(abc, "xyz")); });
    bench ("iends_with fail", [&](){ DoNotOptimize(Strutil::iends_with(abc, "def")); });
}



void
test_case()
{
    std::string s;
    s = "abcDEF,*1";
    Strutil::to_lower(s);
    OIIO_CHECK_EQUAL(s, "abcdef,*1");
    s = "abcDEF,*1";
    Strutil::to_upper(s);
    OIIO_CHECK_EQUAL(s, "ABCDEF,*1");

    s = "abcDEF,*1";
    OIIO_CHECK_EQUAL(Strutil::lower(s), "abcdef,*1");
    OIIO_CHECK_EQUAL (s, "abcDEF,*1");  // make sure it's nondestructive
    Strutil::to_upper(s);
    OIIO_CHECK_EQUAL(Strutil::upper(s), "ABCDEF,*1");
    Strutil::to_upper(s);
}



void
test_strip()
{
    OIIO_CHECK_EQUAL(Strutil::strip("abcdefbac", "abc"), "def");
    OIIO_CHECK_EQUAL(Strutil::strip("defghi", "abc"), "defghi");
    OIIO_CHECK_EQUAL(Strutil::strip("  \tHello, world\n"), "Hello, world");
    OIIO_CHECK_EQUAL(Strutil::strip(" \t"), "");
    OIIO_CHECK_EQUAL(Strutil::strip(""), "");

    OIIO_CHECK_EQUAL(Strutil::lstrip("abcdefbac", "abc"), "defbac");
    OIIO_CHECK_EQUAL(Strutil::lstrip("defghi", "abc"), "defghi");
    OIIO_CHECK_EQUAL(Strutil::lstrip("  \tHello, world\n"), "Hello, world\n");
    OIIO_CHECK_EQUAL(Strutil::lstrip(" \t"), "");
    OIIO_CHECK_EQUAL(Strutil::lstrip(""), "");

    OIIO_CHECK_EQUAL(Strutil::rstrip("abcdefbac", "abc"), "abcdef");
    OIIO_CHECK_EQUAL(Strutil::rstrip("defghi", "abc"), "defghi");
    OIIO_CHECK_EQUAL(Strutil::rstrip("  \tHello, world\n"), "  \tHello, world");
    OIIO_CHECK_EQUAL(Strutil::rstrip(" \t"), "");
    OIIO_CHECK_EQUAL(Strutil::rstrip(""), "");
}



void
test_splits()
{
    std::string s("Now\nis the  time!");
    {   // test default -- split at whitespace
        auto pieces = Strutil::splits(s);
        OIIO_CHECK_EQUAL(pieces.size(), 4);
        OIIO_CHECK_EQUAL(pieces[0], "Now");
        OIIO_CHECK_EQUAL(pieces[1], "is");
        OIIO_CHECK_EQUAL(pieces[2], "the");
        OIIO_CHECK_EQUAL(pieces[3], "time!");
    }
    {   // test custom split string
        auto pieces = Strutil::splits(s, " t");
        OIIO_CHECK_EQUAL(pieces.size(), 3);
        OIIO_CHECK_EQUAL(pieces[0], "Now\nis");
        OIIO_CHECK_EQUAL(pieces[1], "he ");
        OIIO_CHECK_EQUAL(pieces[2], "ime!");
    }
    {   // test split of unfound separator
        auto pieces = Strutil::splits(s, "xyz");
        OIIO_CHECK_EQUAL(pieces.size(), 1);
        OIIO_CHECK_EQUAL(pieces[0], s);
    }
    {   // test maxsplit
        auto pieces = Strutil::splits(s, "", 2);
        OIIO_CHECK_EQUAL(pieces.size(), 2);
        OIIO_CHECK_EQUAL(pieces[0], "Now");
        OIIO_CHECK_EQUAL(pieces[1], "is the  time!");
    }
    {   // test maxsplit with non-default sep
        auto pieces = Strutil::splits(s, " ", 2);
        OIIO_CHECK_EQUAL(pieces.size(), 2);
        OIIO_CHECK_EQUAL(pieces[0], "Now\nis");
        OIIO_CHECK_EQUAL(pieces[1], "the  time!");
    }
    {   // test split against a substring that is not present
        auto pieces = Strutil::splits("blah", "!");
        OIIO_CHECK_EQUAL(pieces.size(), 1);
        OIIO_CHECK_EQUAL(pieces[0], "blah");
    }
    {   // test splitting empty string
        auto pieces = Strutil::splits("", ",");
        OIIO_CHECK_EQUAL(pieces.size(), 0);
    }
    {   // test splitting with empty pieces
        auto pieces = Strutil::splits(",foo,,,bar,", ",");
        OIIO_CHECK_EQUAL(pieces.size(), 6);
        OIIO_CHECK_EQUAL(pieces[0], "");
        OIIO_CHECK_EQUAL(pieces[1], "foo");
        OIIO_CHECK_EQUAL(pieces[2], "");
        OIIO_CHECK_EQUAL(pieces[3], "");
        OIIO_CHECK_EQUAL(pieces[4], "bar");
        OIIO_CHECK_EQUAL(pieces[5], "");
    }
}



void
test_splitsv()
{
    std::string s("Now\nis the  time!");
    {   // test default -- split at whitespace
        auto pieces = Strutil::splitsv(s);
        OIIO_CHECK_EQUAL(pieces.size(), 4);
        OIIO_CHECK_EQUAL(pieces[0], "Now");
        OIIO_CHECK_EQUAL(pieces[1], "is");
        OIIO_CHECK_EQUAL(pieces[2], "the");
        OIIO_CHECK_EQUAL(pieces[3], "time!");
    }
    {   // test custom split string
        auto pieces = Strutil::splitsv(s, " t");
        OIIO_CHECK_EQUAL(pieces.size(), 3);
        OIIO_CHECK_EQUAL(pieces[0], "Now\nis");
        OIIO_CHECK_EQUAL(pieces[1], "he ");
        OIIO_CHECK_EQUAL(pieces[2], "ime!");
    }
    {   // test split of unfound separator
        auto pieces = Strutil::splitsv(s, "xyz");
        OIIO_CHECK_EQUAL(pieces.size(), 1);
        OIIO_CHECK_EQUAL(pieces[0], s);
    }
    {   // test maxsplit
        auto pieces = Strutil::splitsv(s, "", 2);
        OIIO_CHECK_EQUAL(pieces.size(), 2);
        OIIO_CHECK_EQUAL(pieces[0], "Now");
        OIIO_CHECK_EQUAL(pieces[1], "is the  time!");
    }
    {   // test maxsplit with non-default sep
        auto pieces = Strutil::splitsv(s, " ", 2);
        OIIO_CHECK_EQUAL(pieces.size(), 2);
        OIIO_CHECK_EQUAL(pieces[0], "Now\nis");
        OIIO_CHECK_EQUAL(pieces[1], "the  time!");
    }
    {   // test split against a substring that is not present
        auto pieces = Strutil::splitsv("blah", "!");
        OIIO_CHECK_EQUAL(pieces.size(), 1);
        OIIO_CHECK_EQUAL(pieces[0], "blah");
    }
    {   // test splitting empty string
        auto pieces = Strutil::splitsv("", ",");
        OIIO_CHECK_EQUAL(pieces.size(), 0);
    }
    {   // test splitting with empty pieces
        auto pieces = Strutil::splitsv(",foo,,,bar,", ",");
        OIIO_CHECK_EQUAL(pieces.size(), 6);
        OIIO_CHECK_EQUAL(pieces[0], "");
        OIIO_CHECK_EQUAL(pieces[1], "foo");
        OIIO_CHECK_EQUAL(pieces[2], "");
        OIIO_CHECK_EQUAL(pieces[3], "");
        OIIO_CHECK_EQUAL(pieces[4], "bar");
        OIIO_CHECK_EQUAL(pieces[5], "");
    }
}



void
test_join()
{
    std::vector<std::string> strvec { "Now", "is", "the", "time" };
    OIIO_CHECK_EQUAL(Strutil::join(strvec, ". "), "Now. is. the. time");

    std::vector<string_view> svvec { "Now", "is", "the", "time" };
    OIIO_CHECK_EQUAL(Strutil::join(svvec, "/"), "Now/is/the/time");

    std::vector<int> intvec { 3, 2, 1 };
    OIIO_CHECK_EQUAL(Strutil::join(intvec, " "), "3 2 1");

    int intarr[] = { 4, 2 };
    OIIO_CHECK_EQUAL(Strutil::join(intarr, ","), "4,2");

    // Test join's `len` parameter.
    float farr[] = { 1, 2, 3.5, 4, 5 };
    OIIO_CHECK_EQUAL(Strutil::join(farr, ",", 3), "1,2,3.5");
    OIIO_CHECK_EQUAL(Strutil::join(farr, ",", 7), "1,2,3.5,4,5,0,0");
}



void
test_concat()
{
    std::cout << "Testing concat\n";
    OIIO_CHECK_EQUAL(Strutil::concat("foo", "bar"), "foobar");
    OIIO_CHECK_EQUAL(Strutil::concat("foo", ""), "foo");
    OIIO_CHECK_EQUAL(Strutil::concat("", "foo"), "foo");
    OIIO_CHECK_EQUAL(Strutil::concat("", ""), "");
    std::string longstring(Strutil::repeat("01234567890", 100));
    OIIO_CHECK_EQUAL(Strutil::concat(longstring, longstring),
                     Strutil::sprintf("%s%s", longstring, longstring));
    OIIO_CHECK_EQUAL(Strutil::concat(longstring, longstring),
                     Strutil::fmt::format("{}{}", longstring, longstring));

    Benchmarker bench;
    bench.indent (2);
    bench.units (Benchmarker::Unit::ns);
    std::string foostr("foo"), barstr("bar");
    bench ("concat literal short+short", [&](){ return DoNotOptimize(Strutil::concat("foo", "bar")); });
    bench ("concat literal long+short", [&](){ return DoNotOptimize(Strutil::concat(longstring, "bar")); });
    bench ("concat literal long+long", [&](){ return DoNotOptimize(Strutil::concat(longstring, longstring)); });
    bench ("format literal short+short", [&](){ return DoNotOptimize(Strutil::fmt::format("{}{}", "foo", "bar")); });
    bench ("format literal long+short", [&](){ return DoNotOptimize(Strutil::fmt::format("{}{}", longstring, "bar")); });
    bench ("format literal long+long", [&](){ return DoNotOptimize(Strutil::fmt::format("{}{}", longstring, longstring)); });
    bench ("sprintf literal short+short", [&](){ return DoNotOptimize(Strutil::sprintf("%s%s", "foo", "bar")); });
    bench ("sprintf literal long+short", [&](){ return DoNotOptimize(Strutil::sprintf("%s%s", longstring, "bar")); });
    bench ("sprintf literal long+long", [&](){ return DoNotOptimize(Strutil::sprintf("%s%s", longstring, longstring)); });

    bench ("concat str short+short", [&](){ return DoNotOptimize(Strutil::concat(foostr, barstr)); });
    bench ("concat str long+short", [&](){ return DoNotOptimize(Strutil::concat(longstring, barstr)); });
    bench ("concat str long+long", [&](){ return DoNotOptimize(Strutil::concat(longstring, longstring)); });
    bench ("format str short+short", [&](){ return DoNotOptimize(Strutil::fmt::format("{}{}", foostr, barstr)); });
    bench ("format str long+short", [&](){ return DoNotOptimize(Strutil::fmt::format("{}{}", longstring, barstr)); });
    bench ("format str long+long", [&](){ return DoNotOptimize(Strutil::fmt::format("{}{}", longstring, longstring)); });
    bench ("sprintf str short+short", [&](){ return DoNotOptimize(Strutil::sprintf("%s%s", foostr, barstr)); });
    bench ("sprintf str long+short", [&](){ return DoNotOptimize(Strutil::sprintf("%s%s", longstring, barstr)); });
    bench ("sprintf str long+long", [&](){ return DoNotOptimize(Strutil::sprintf("%s%s", longstring, longstring)); });
    bench ("std::string + literal short+short", [&](){ return DoNotOptimize(std::string("foo") + std::string("bar")); });
    bench ("std::string + literal long+short", [&](){ return DoNotOptimize(longstring + std::string("bar")); });
    bench ("std::string + literal long+long", [&](){ return DoNotOptimize(longstring + longstring); });
    bench ("std::string + str short+short", [&](){ return DoNotOptimize(foostr + barstr); });
    bench ("std::string + str long+short", [&](){ return DoNotOptimize(longstring + barstr); });
    bench ("std::string + str long+long", [&](){ return DoNotOptimize(longstring + longstring); });
}



void
test_repeat()
{
    std::cout << "Testing repeat\n";
    OIIO_CHECK_EQUAL(Strutil::repeat("foo", 3), "foofoofoo");
    OIIO_CHECK_EQUAL(Strutil::repeat("foo", 1), "foo");
    OIIO_CHECK_EQUAL(Strutil::repeat("foo", 0), "");
    OIIO_CHECK_EQUAL(Strutil::repeat("foo", -1), "");
    OIIO_CHECK_EQUAL(Strutil::repeat("0123456789", 100),
                     Strutil::repeat("01234567890123456789", 50));
}



void
test_replace()
{
    std::cout << "Testing replace\n";
    std::string pattern("Red rose, red rose, end.");
    // Replace start
    OIIO_CHECK_EQUAL(Strutil::replace(pattern, "Red", "foo"),
                     "foo rose, red rose, end.");
    // Replace end
    OIIO_CHECK_EQUAL(Strutil::replace(pattern, "end.", "foo"),
                     "Red rose, red rose, foo");
    // Pattern not found
    OIIO_CHECK_EQUAL(Strutil::replace(pattern, "bar", "foo"), pattern);
    // One replacement
    OIIO_CHECK_EQUAL(Strutil::replace(pattern, "rose", "foo"),
                     "Red foo, red rose, end.");
    // Global replacement
    OIIO_CHECK_EQUAL(Strutil::replace(pattern, "rose", "foo", true),
                     "Red foo, red foo, end.");
}



void
test_excise_string_after_head()
{
    std::cout << "Testing excise_string_after_head\n";
    std::string pattern = "Red rose, red rose, end.";

    // test non-match
    {
        std::string p = pattern;
        auto m = Strutil::excise_string_after_head(p, "blue");
        OIIO_CHECK_EQUAL(p, pattern);
        OIIO_CHECK_EQUAL(m, "");
    }

    // test match: head is "ro", match subsequent chars to the next space
    {
        std::string p = pattern;
        auto m = Strutil::excise_string_after_head(p, "ro");
        OIIO_CHECK_EQUAL(p, "Red red rose, end.");
        OIIO_CHECK_EQUAL(m, "se,");
    }
}



void
test_numeric_conversion()
{
    std::cout << "Testing string_is, string_from conversions\n";
    size_t pos;

    OIIO_CHECK_EQUAL(Strutil::string_is_int("142"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("-142"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("+142"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("142.0"), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_int(""), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("  "), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("foo"), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("142x"), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_int(" 142"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("142 "), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_int("x142"), false);

    OIIO_CHECK_EQUAL(Strutil::string_is_float("142"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_float("142.0"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_float(""), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_float("  "), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_float("foo"), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_float("142x"), false);
    OIIO_CHECK_EQUAL(Strutil::string_is_float(" 142"), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_float(" 142 "), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_float(" 142.0 "), true);
    OIIO_CHECK_EQUAL(Strutil::string_is_float("x142"), false);

    // Note: we don't test string_is<> separately because it's just
    // implemented directly as calls to string_is_{int,float}.

    OIIO_CHECK_EQUAL(Strutil::stoi("hi"), 0);
    OIIO_CHECK_EQUAL(Strutil::stoi("  "), 0);
    OIIO_CHECK_EQUAL(Strutil::stoi("123"), 123);
    OIIO_CHECK_EQUAL(Strutil::stoi("-123"), -123);
    OIIO_CHECK_EQUAL(Strutil::stoi("+123"), 123);
    OIIO_CHECK_EQUAL(Strutil::stoi(" 123 "), 123);
    OIIO_CHECK_EQUAL(Strutil::stoi("123.45"), 123);
    OIIO_CHECK_EQUAL(Strutil::stoi("12345678901234567890"),
                     std::numeric_limits<int>::max());
    OIIO_CHECK_EQUAL(Strutil::stoi("-12345678901234567890"),
                     std::numeric_limits<int>::min());
    OIIO_CHECK_EQUAL(Strutil::stoi("0x100", nullptr, 16), 256);  // hex
    OIIO_CHECK_EQUAL(Strutil::stoi("25555555555555555551"), 2147483647);

    OIIO_CHECK_EQUAL(Strutil::stoui("hi"), 0);
    OIIO_CHECK_EQUAL(Strutil::stoui("  "), 0);
    OIIO_CHECK_EQUAL(Strutil::stoui("123"), 123);
    OIIO_CHECK_EQUAL(Strutil::stoui("+123"), 123);
    OIIO_CHECK_EQUAL(Strutil::stoui(" 123 "), 123);
    OIIO_CHECK_EQUAL(Strutil::stoui("123.45"), 123);
    // bigger than fits in an int, to be sure we're really using uint:
    OIIO_CHECK_EQUAL(Strutil::stoui("3221225472"), 3221225472UL);

    OIIO_CHECK_EQUAL(Strutil::stoi("hi", &pos), 0);
    OIIO_CHECK_EQUAL(pos, 0);
    OIIO_CHECK_EQUAL(Strutil::stoi("  ", &pos), 0);
    OIIO_CHECK_EQUAL(pos, 0);
    OIIO_CHECK_EQUAL(Strutil::stoi("123", &pos), 123);
    OIIO_CHECK_EQUAL(pos, 3);
    OIIO_CHECK_EQUAL(Strutil::stoi("-123", &pos), -123);
    OIIO_CHECK_EQUAL(pos, 4);
    OIIO_CHECK_EQUAL(Strutil::stoi(" 123 ", &pos), 123);
    OIIO_CHECK_EQUAL(pos, 4);
    OIIO_CHECK_EQUAL(Strutil::stoi("123.45", &pos), 123);
    OIIO_CHECK_EQUAL(pos, 3);

#if 0
    // Make sure it's correct for EVERY value. This takes too long to do as
    // part of unit tests, but I assure you that I did it once to confirm.
    for (int64_t i = std::numeric_limits<int>::min(); i <= std::numeric_limits<int>::max(); ++i)
        OIIO_CHECK_EQUAL (Strutil::stoi(Strutil::sprintf("%d",i)), i);
#endif

    OIIO_CHECK_EQUAL(Strutil::stoui("hi"), unsigned(0));
    OIIO_CHECK_EQUAL(Strutil::stoui("  "), unsigned(0));
    OIIO_CHECK_EQUAL(Strutil::stoui("123"), unsigned(123));
    OIIO_CHECK_EQUAL(Strutil::stoui("-123"), unsigned(-123));
    OIIO_CHECK_EQUAL(Strutil::stoui(" 123 "), unsigned(123));
    OIIO_CHECK_EQUAL(Strutil::stoui("123.45"), unsigned(123));

    OIIO_CHECK_EQUAL(Strutil::stof("hi"), 0.0f);
    OIIO_CHECK_EQUAL(Strutil::stof("  "), 0.0f);
    OIIO_CHECK_EQUAL(Strutil::stof("123"), 123.0f);
    OIIO_CHECK_EQUAL(Strutil::stof("-123"), -123.0f);
    OIIO_CHECK_EQUAL(Strutil::stof("123.45"), 123.45f);
    OIIO_CHECK_EQUAL(Strutil::stof("123.45xyz"), 123.45f);
    OIIO_CHECK_EQUAL(Strutil::stof(" 123.45 "), 123.45f);
    OIIO_CHECK_EQUAL(Strutil::stof("123.45+12"), 123.45f);
    OIIO_CHECK_EQUAL(Strutil::stof("1.2345e+2"), 123.45f);

    OIIO_CHECK_EQUAL(Strutil::stof("hi", &pos), 0.0f);
    OIIO_CHECK_EQUAL(pos, 0);
    OIIO_CHECK_EQUAL(Strutil::stof("  ", &pos), 0.0f);
    OIIO_CHECK_EQUAL(pos, 0);
    OIIO_CHECK_EQUAL(Strutil::stof("123", &pos), 123.0f);
    OIIO_CHECK_EQUAL(pos, 3);
    OIIO_CHECK_EQUAL(Strutil::stof("-123", &pos), -123.0f);
    OIIO_CHECK_EQUAL(pos, 4);
    OIIO_CHECK_EQUAL(Strutil::stof("123.45", &pos), 123.45f);
    OIIO_CHECK_EQUAL(pos, 6);
    OIIO_CHECK_EQUAL(Strutil::stof("123.45xyz", &pos), 123.45f);
    OIIO_CHECK_EQUAL(pos, 6);
    OIIO_CHECK_EQUAL(Strutil::stof(" 123.45 ", &pos), 123.45f);
    OIIO_CHECK_EQUAL(pos, 7);
    OIIO_CHECK_EQUAL(Strutil::stof("123.45+12", &pos), 123.45f);
    OIIO_CHECK_EQUAL(pos, 6);
    OIIO_CHECK_EQUAL(Strutil::stof("1.2345e2", &pos), 123.45f);
    OIIO_CHECK_EQUAL(pos, 8);
    // stress case!
    OIIO_CHECK_EQUAL (Strutil::stof("100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001E-200"), 1.0f);
    OIIO_CHECK_EQUAL (Strutil::stof("0.00000000000000000001"), 1.0e-20f);

    OIIO_CHECK_EQUAL(Strutil::strtod("314.25"), 314.25);
    OIIO_CHECK_EQUAL(Strutil::strtod("hi"), 0.0);

    pos = 100;
    OIIO_CHECK_EQUAL(Strutil::stod(string_view("314.25"), &pos), 314.25);
    OIIO_CHECK_EQUAL(pos, 6);
    pos = 100;
    OIIO_CHECK_EQUAL(Strutil::stod("hi", &pos), 0.0);
    OIIO_CHECK_EQUAL(pos, 0);
    pos = 100;
    OIIO_CHECK_EQUAL(Strutil::stod("", &pos), 0.0);
    OIIO_CHECK_EQUAL(pos, 0);
    pos = 100;
    OIIO_CHECK_EQUAL(Strutil::stod(nullptr, &pos), 0.0);
    OIIO_CHECK_EQUAL(pos, 0);

    // Note: we don't test from_strings<> separately because it's just
    // implemented directly as calls to stoi, stoui, stof.

    Benchmarker bench;
    bench.indent (2);
    bench.units (Benchmarker::Unit::ns);
    const char* numcstr = "123.45";
    std::string numstring (numcstr);
    bench ("get default locale", [](){ std::locale loc; DoNotOptimize (loc); });
    bench ("ref classic locale", [](){ DoNotOptimize (std::locale::classic()); });
    bench ("std atoi", [&](){ DoNotOptimize(atoi(numcstr));}); // NOLINT(cert-err34-c)
    bench ("Strutil::stoi(string) ", [&](){ return DoNotOptimize(Strutil::stoi(numstring)); });
    bench ("Strutil::stoi(char*) ", [&](){ return DoNotOptimize(Strutil::stoi(numcstr)); });
    bench ("Strutil::stoui(char*) ", [&](){ return DoNotOptimize(Strutil::stoui(numcstr)); });
    bench ("std atof", [&](){ DoNotOptimize(atof(numcstr));}); // NOLINT(cert-err34-c)
    bench ("std strtod", [&](){ DoNotOptimize(::strtod(numcstr, nullptr));});
    bench ("Strutil::from_string<float>", [&](){ DoNotOptimize(Strutil::from_string<float>(numstring));});
    bench ("Strutil::stof(string) - locale-independent", [&](){ return DoNotOptimize(Strutil::stof(numstring)); });
    bench ("Strutil::stof(char*) - locale-independent", [&](){ return DoNotOptimize(Strutil::stof(numcstr)); });
    bench ("Strutil::stof(string_view) - locale-independent", [&](){ return DoNotOptimize(Strutil::stof(string_view(numstring))); });
    bench ("locale switch (to classic)", [&](){ std::locale::global (std::locale::classic()); });
}



void
test_to_string()
{
    std::cout << "Testing to_string\n";
    OIIO_CHECK_EQUAL(Strutil::to_string(3.14f), "3.14");
    OIIO_CHECK_EQUAL(Strutil::to_string(42), "42");
    OIIO_CHECK_EQUAL(Strutil::to_string("hi"), "hi");
    OIIO_CHECK_EQUAL(Strutil::to_string(std::string("hello")), "hello");
    OIIO_CHECK_EQUAL(Strutil::to_string(string_view("hey")), "hey");
    OIIO_CHECK_EQUAL(Strutil::to_string(ustring("yo")), "yo");
}



void
test_extract()
{
    std::cout << "Testing extract_from_list_string\n";
    std::vector<int> vals;
    int n;

    vals.clear();
    vals.resize(3, -1);
    n = Strutil::extract_from_list_string(vals, "1");
    OIIO_CHECK_EQUAL(vals, std::vector<int>({ 1, 1, 1 }));
    OIIO_CHECK_EQUAL(n, 1);

    vals.clear();
    vals.resize(3, -1);
    n = Strutil::extract_from_list_string(vals, "1,3,5");
    OIIO_CHECK_EQUAL(vals, std::vector<int>({ 1, 3, 5 }));
    OIIO_CHECK_EQUAL(n, 3);

    vals.clear();
    vals.resize(3, -1);
    n = Strutil::extract_from_list_string(vals, "1,,5");
    OIIO_CHECK_EQUAL(vals, std::vector<int>({ 1, -1, 5 }));
    OIIO_CHECK_EQUAL(n, 3);

    vals.clear();
    vals.resize(3, -1);
    n = Strutil::extract_from_list_string(vals, "abc");
    OIIO_CHECK_EQUAL(vals, std::vector<int>({ 0, 0, 0 }));
    OIIO_CHECK_EQUAL(n, 1);

    vals.clear();
    vals.resize(3, -1);
    n = Strutil::extract_from_list_string(vals, "");
    OIIO_CHECK_EQUAL(vals, std::vector<int>({ -1, -1, -1 }));
    OIIO_CHECK_EQUAL(n, 0);

    vals.clear();
    n = Strutil::extract_from_list_string(vals, "1,3,5");
    OIIO_CHECK_EQUAL(vals, std::vector<int>({ 1, 3, 5 }));
    OIIO_CHECK_EQUAL(n, 3);

    // Make sure that the vector-returning version works
    OIIO_CHECK_EQUAL(Strutil::extract_from_list_string<float>("1", 3, -1.0f),
                     std::vector<float>({ 1, 1, 1 }));
    OIIO_CHECK_EQUAL(
        Strutil::extract_from_list_string<float>("1,3,5", 3, -1.0f),
        std::vector<float>({ 1, 3, 5 }));
    OIIO_CHECK_EQUAL(Strutil::extract_from_list_string<float>("1,,5", 3, -1.0f),
                     std::vector<float>({ 1, -1, 5 }));
    OIIO_CHECK_EQUAL(Strutil::extract_from_list_string<float>("abc", 3, -1.0f),
                     std::vector<float>({ 0, 0, 0 }));
    OIIO_CHECK_EQUAL(Strutil::extract_from_list_string<float>("", 3, -1.0f),
                     std::vector<float>({ -1, -1, -1 }));
    OIIO_CHECK_EQUAL(Strutil::extract_from_list_string<float>("1,3,5"),
                     std::vector<float>({ 1, 3, 5 }));
    OIIO_CHECK_EQUAL(Strutil::extract_from_list_string<float>("1,3,5,7"),
                     std::vector<float>({ 1, 3, 5, 7 }));
}



void
test_safe_strcpy()
{
    std::cout << "Testing safe_strcpy\n";
    {  // test in-bounds copy
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy(result, "A", 3);
        OIIO_CHECK_EQUAL(result[0], 'A');
        OIIO_CHECK_EQUAL(result[1], 0);
        OIIO_CHECK_EQUAL(result[2], 0);
        OIIO_CHECK_EQUAL(result[3], '3');
    }
    {  // test over-bounds copy
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy(result, "ABC", 3);
        OIIO_CHECK_EQUAL(result[0], 'A');
        OIIO_CHECK_EQUAL(result[1], 'B');
        OIIO_CHECK_EQUAL(result[2], 0);
        OIIO_CHECK_EQUAL(result[3], '3');
    }
    {  // test empty string copy
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy(result, "", 3);
        OIIO_CHECK_EQUAL(result[0], 0);
        OIIO_CHECK_EQUAL(result[1], 0);
        OIIO_CHECK_EQUAL(result[2], 0);
        OIIO_CHECK_EQUAL(result[3], '3');
    }
    {  // test NULL case
        char result[4] = { '0', '1', '2', '3' };
        Strutil::safe_strcpy(result, NULL, 3);
        OIIO_CHECK_EQUAL(result[0], 0);
        OIIO_CHECK_EQUAL(result[1], 0);
        OIIO_CHECK_EQUAL(result[2], 0);
        OIIO_CHECK_EQUAL(result[3], '3');
    }
}



void
test_safe_strcat()
{
    std::cout << "Testing safe_strcat\n";
    const size_t len = 8;
    {  // test in-bounds copy
        char result[len + 1] = { 100, 101, 102, 103, 104, 105, 106, 107, 108 };
        Strutil::safe_strcpy(result, "123", len);
        Strutil::safe_strcat(result, "ABC", len);
        OIIO_CHECK_EQUAL(std::string(result), "123ABC");
        OIIO_CHECK_EQUAL(result[6], 0);
        OIIO_CHECK_EQUAL(result[7], 0);
        OIIO_CHECK_EQUAL(result[8], 108);
    }
    {  // test over-bounds copy
        char result[len + 1] = { 100, 101, 102, 103, 104, 105, 106, 107, 108 };
        Strutil::safe_strcpy(result, "123", len);
        Strutil::safe_strcat(result, "ABCDEF", len);
        OIIO_CHECK_EQUAL(std::string(result), "123ABCD");
        OIIO_CHECK_EQUAL(result[7], 0);
        OIIO_CHECK_EQUAL(result[8], 108);
    }
    {  // test empty string copy
        char result[len + 1] = { 100, 101, 102, 103, 104, 105, 106, 107, 108 };
        Strutil::safe_strcpy(result, "123", len);
        Strutil::safe_strcat(result, "", len);
        OIIO_CHECK_EQUAL(std::string(result), "123");
        OIIO_CHECK_EQUAL(result[3], 0);
        OIIO_CHECK_EQUAL(result[4], 0);
        OIIO_CHECK_EQUAL(result[5], 0);
        OIIO_CHECK_EQUAL(result[6], 0);
        OIIO_CHECK_EQUAL(result[7], 0);
        OIIO_CHECK_EQUAL(result[8], 108);
    }
    {  // test NULL case
        char result[len + 1] = { 100, 101, 102, 103, 104, 105, 106, 107, 108 };
        Strutil::safe_strcpy(result, "123", len);
        Strutil::safe_strcat(result, nullptr, len);
        OIIO_CHECK_EQUAL(std::string(result), "123");
        OIIO_CHECK_EQUAL(result[3], 0);
        OIIO_CHECK_EQUAL(result[4], 0);
        OIIO_CHECK_EQUAL(result[5], 0);
        OIIO_CHECK_EQUAL(result[6], 0);
        OIIO_CHECK_EQUAL(result[7], 0);
        OIIO_CHECK_EQUAL(result[8], 108);
    }
}



// test safe_strlen and closely related safe_string_view, safe_string
void
test_safe_strlen()
{
    char a[] = "012";  // expected
    char b[] = "012" "\0" "456789";  // nul embedded in the string
    char c[] = "0123456789001234567890";  // long string
    char d[] = "";  // empty string

    Strutil::print("Testing safe_strlen\n");
    OIIO_CHECK_EQUAL(Strutil::safe_strlen(a, 10), 3);
    OIIO_CHECK_EQUAL(Strutil::safe_strlen(b, 10), 3);
    OIIO_CHECK_EQUAL(Strutil::safe_strlen(c, 10), 10);
    OIIO_CHECK_EQUAL(Strutil::safe_strlen(d, 10), 0);

    std::cout << "Testing safe_string_view\n";
    OIIO_CHECK_EQUAL(Strutil::safe_string_view(a, 10), string_view("012"));
    OIIO_CHECK_EQUAL(Strutil::safe_string_view(b, 10), string_view("012"));
    OIIO_CHECK_EQUAL(Strutil::safe_string_view(c, 10), string_view("0123456789"));
    OIIO_CHECK_EQUAL(Strutil::safe_string_view(d, 10), string_view(""));

    std::cout << "Testing safe_string\n";
    OIIO_CHECK_EQUAL(Strutil::safe_string(a, 10), std::string("012"));
    OIIO_CHECK_EQUAL(Strutil::safe_string(b, 10), std::string("012"));
    OIIO_CHECK_EQUAL(Strutil::safe_string(c, 10), std::string("0123456789"));
    OIIO_CHECK_EQUAL(Strutil::safe_string(d, 10), std::string(""));
}



// test some of the trickier methods in string_view.
void
test_string_view()
{
    std::cout << "Testing string_view methods\n";
    const char* cstr = "0123401234";
    std::string s(cstr);
    string_view sr(s);

    OIIO_CHECK_EQUAL(string_view(), "");  // Default ctr should be empty
    OIIO_CHECK_EQUAL(string_view(cstr), cstr);  // Test ctr from char*
    OIIO_CHECK_EQUAL(string_view(s), cstr);  // test ctr from std::string
    OIIO_CHECK_EQUAL(sr, cstr);  // These better be the same

#ifdef OIIO_STD_STRING_VIEW_AVAILABLE
    {
        std::cout << "  Testing OIIO::string_view <-> std::string_view\n";
        std::string_view ssv = sr;
        OIIO::string_view osv = ssv;
        OIIO_CHECK_EQUAL(osv, sr);
    }
#endif

#ifdef OIIO_EXPERIMENTAL_STRING_VIEW_AVAILABLE
    {
        std::cout << "  Testing OIIO::string_view <-> std::experimental::string_view\n";
        std::experimental::string_view ssv = sr;
        OIIO::string_view osv = ssv;
        OIIO_CHECK_EQUAL(osv, sr);
    }
#endif

    OIIO_CHECK_EQUAL(sr.substr(0), sr); // whole string
    OIIO_CHECK_EQUAL(sr.substr(2), "23401234"); // nonzero pos, default n
    OIIO_CHECK_EQUAL(sr.substr(2, 3), "234"); // true substrng
    OIIO_CHECK_EQUAL(sr.substr(string_view::npos, 3), ""); // npos start
    OIIO_CHECK_EQUAL(sr.substr(3, 0), ""); // zero length
    OIIO_CHECK_EQUAL(sr.substr(10, 3), ""); // start at end
    OIIO_CHECK_EQUAL(sr.substr(18, 3), ""); // start past end
    OIIO_CHECK_EQUAL(sr.substr(4, 18), "401234"); // end too big

    OIIO_CHECK_EQUAL(sr.find("123"), s.find("123"));
    OIIO_CHECK_EQUAL(sr.find("123"), 1);
    OIIO_CHECK_EQUAL(sr.find("143"), string_view::npos);
    OIIO_CHECK_EQUAL(sr.find("123", 4), s.find("123", 4));
    OIIO_CHECK_EQUAL(sr.find("123", 4), 6);
    OIIO_CHECK_EQUAL(sr.find("143", 4), string_view::npos);
    OIIO_CHECK_EQUAL(sr.find(""), s.find(""));
    OIIO_CHECK_EQUAL(sr.find(""), 0);
    OIIO_CHECK_EQUAL(string_view("").find(""), string_view::npos);

    OIIO_CHECK_EQUAL(sr.find('1'), s.find('1'));
    OIIO_CHECK_EQUAL(sr.find('1'), 1);
    OIIO_CHECK_EQUAL(sr.find('5'), string_view::npos);
    OIIO_CHECK_EQUAL(sr.find('1', 4), s.find('1', 4));
    OIIO_CHECK_EQUAL(sr.find('1', 4), 6);
    OIIO_CHECK_EQUAL(sr.find('5', 4), string_view::npos);

    OIIO_CHECK_EQUAL(sr.rfind("123"), s.rfind("123"));
    OIIO_CHECK_EQUAL(sr.rfind("123"), 6);
    OIIO_CHECK_EQUAL(sr.rfind("1234"), 6);
    OIIO_CHECK_EQUAL(sr.rfind("143"), string_view::npos);
    OIIO_CHECK_EQUAL(sr.rfind("123", 5), s.rfind("123", 5));
    OIIO_CHECK_EQUAL(sr.rfind("123", 5), 1);
    OIIO_CHECK_EQUAL(sr.rfind("123", 4), 1);
    OIIO_CHECK_EQUAL(sr.rfind("143", 5), string_view::npos);
    OIIO_CHECK_EQUAL(sr.rfind("012", 4), 0);
    OIIO_CHECK_EQUAL(sr.rfind(""), s.rfind(""));
    OIIO_CHECK_EQUAL(sr.rfind(""), 10);
    OIIO_CHECK_EQUAL(string_view("").rfind(""), string_view::npos);

    OIIO_CHECK_EQUAL(sr.rfind('1'), s.rfind('1'));
    OIIO_CHECK_EQUAL(sr.rfind('1'), 6);
    OIIO_CHECK_EQUAL(sr.rfind('5'), string_view::npos);
    OIIO_CHECK_EQUAL(sr.rfind('1', 4), s.rfind('1', 4));
    OIIO_CHECK_EQUAL(sr.rfind('1', 4), 1);
    OIIO_CHECK_EQUAL(sr.rfind('5', 4), string_view::npos);

    OIIO_CHECK_EQUAL(sr.find_first_of('2'), 2);
    OIIO_CHECK_EQUAL(sr.find_first_of("23"), 2);
    OIIO_CHECK_EQUAL(sr.find_first_of("xyz"), string_view::npos);
    OIIO_CHECK_EQUAL(sr.find_first_of('2', 5), 7);
    OIIO_CHECK_EQUAL(sr.find_first_of("23", 5), 7);

    OIIO_CHECK_EQUAL(sr.find_last_of('2'), 7);
    OIIO_CHECK_EQUAL(sr.find_last_of("23"), 8);
    OIIO_CHECK_EQUAL(sr.find_last_of("xyz"), string_view::npos);
    OIIO_CHECK_EQUAL(sr.find_last_of('2', 5), 2);
    OIIO_CHECK_EQUAL(sr.find_last_of("23", 5), 3);

    OIIO_CHECK_EQUAL(sr.find_first_not_of('0'), 1);
    OIIO_CHECK_EQUAL(sr.find_first_not_of("012"), 3);
    OIIO_CHECK_EQUAL(sr.find_first_not_of('0', 5), 6);
    OIIO_CHECK_EQUAL(sr.find_first_not_of("012", 5), 8);
    OIIO_CHECK_EQUAL(sr.find_first_of("xyz"), string_view::npos);

    OIIO_CHECK_EQUAL(sr.find_last_not_of('4'), 8);
    OIIO_CHECK_EQUAL(sr.find_last_not_of("234"), 6);
    OIIO_CHECK_EQUAL(sr.find_last_not_of('4', 5), 3);
    OIIO_CHECK_EQUAL(sr.find_last_not_of("234", 5), 1);
    OIIO_CHECK_EQUAL(sr.find_last_not_of("xyz"), 9);
    OIIO_CHECK_EQUAL(sr.find_last_not_of("01234"), string_view::npos);

    // Just force template expansion of wchar_t variety to make sure it's
    // not horribly broken.
    wstring_view wsv;
    OIIO_CHECK_ASSERT(wsv == wsv);

    // Test the freestanding OIIO::c_str() function
    OIIO_CHECK_EQUAL(OIIO::c_str(""), std::string());
    OIIO_CHECK_EQUAL(OIIO::c_str(cstr), cstr);
    OIIO_CHECK_EQUAL(OIIO::c_str(s), s);
    OIIO_CHECK_EQUAL(OIIO::c_str(ustring(cstr)), ustring(cstr));
    OIIO_CHECK_EQUAL(OIIO::c_str(sr), sr);
    OIIO_CHECK_EQUAL(OIIO::c_str(string_view(sr.data(), 2)), std::string("01"));
    Strutil::print("addr cstr={:p}, s={:p}, ustring={:p}, sr={:p}, c_str(sr)={:p}\n",
                     (void*)cstr, (void*)s.c_str(), (void*)ustring(cstr).c_str(), (void*)sr.data(),
                     (void*)c_str(sr));
}



void test_parse ()
{
    using namespace Strutil;
    std::cout << "Testing parse functions\n";
    string_view s;
    s = "";        skip_whitespace(s);  OIIO_CHECK_EQUAL (s, "");
    s = "   ";     skip_whitespace(s);  OIIO_CHECK_EQUAL (s, "");
    s = "foo";     skip_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo");
    s = "\tfoo\t"; skip_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo\t");
    s = "  foo  "; skip_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo  ");

    s = "";        remove_trailing_whitespace(s);  OIIO_CHECK_EQUAL (s, "");
    s = "   ";     remove_trailing_whitespace(s);  OIIO_CHECK_EQUAL (s, "");
    s = "foo";     remove_trailing_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo");
    s = "\tfoo\t"; remove_trailing_whitespace(s);  OIIO_CHECK_EQUAL (s, "\tfoo");
    s = "  foo  "; remove_trailing_whitespace(s);  OIIO_CHECK_EQUAL (s, "  foo");

    s = "";        trim_whitespace(s);  OIIO_CHECK_EQUAL (s, "");
    s = "   ";     trim_whitespace(s);  OIIO_CHECK_EQUAL (s, "");
    s = "foo";     trim_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo");
    s = "\tfoo\t"; trim_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo");
    s = "  foo  "; trim_whitespace(s);  OIIO_CHECK_EQUAL (s, "foo");
   
    OIIO_CHECK_EQUAL(trimmed_whitespace(""),        "");
    OIIO_CHECK_EQUAL(trimmed_whitespace("   "),     "");
    OIIO_CHECK_EQUAL(trimmed_whitespace("foo"),     "foo");
    OIIO_CHECK_EQUAL(trimmed_whitespace("\tfoo\t"), "foo");
    OIIO_CHECK_EQUAL(trimmed_whitespace("  foo  "), "foo");
   
    s = "abc"; OIIO_CHECK_ASSERT (! parse_char (s, 'd') && s == "abc");

    s = "abc"; OIIO_CHECK_ASSERT (parse_char (s, 'a', true, false) && s == "abc");
    s = "abc"; OIIO_CHECK_ASSERT (parse_char (s, 'a') && s == "bc");

    s = "abc"; OIIO_CHECK_ASSERT (parse_until_char (s, 'c', false) && s == "abc");
    s = "abc"; OIIO_CHECK_ASSERT (parse_until_char (s, 'c') && s == "c");
    s = "abc"; OIIO_CHECK_ASSERT (! parse_until_char (s, 'd') && s == "");

    s = "abcdef";
    OIIO_CHECK_ASSERT (! parse_prefix (s, "def", false) && s == "abcdef");
    OIIO_CHECK_ASSERT (parse_prefix (s, "abc", false) && s == "abcdef");
    OIIO_CHECK_ASSERT (parse_prefix (s, "abc") && s == "def");

    int i = 0;
    s = "abc"; OIIO_CHECK_ASSERT (! parse_int (s, i) && s == "abc");
    s = " 143 abc"; OIIO_CHECK_ASSERT (parse_int (s, i) && i == 143 && s == " abc");
    s = " 143 abc"; OIIO_CHECK_ASSERT (parse_int (s, i, false) && i == 143 && s == " 143 abc");

    float f = 0;
    s = "abc"; OIIO_CHECK_ASSERT (! parse_float (s, f) && s == "abc");
    s = " 42.1 abc"; OIIO_CHECK_ASSERT (parse_float (s, f) && f == 42.1f && s == " abc");
    s = " 42.1 abc"; OIIO_CHECK_ASSERT (parse_float (s, f, false) && f == 42.1f && s == " 42.1 abc");

    {
        string_view sv;
        float xyz[3] = { 0, 0, 0 };
        sv = "xxx 1 2 3 4 5 6";
        OIIO_CHECK_ASSERT(parse_values(sv, "xxx", xyz, "", "4")
                          && xyz[0] == 1 && xyz[1] == 2 && xyz[2] == 3
                          && sv == " 5 6");
        sv = "xxx 1 2 3 4 5 6";
        OIIO_CHECK_ASSERT(!parse_values(sv, "", xyz));
        sv = "xxx 1 2 3 4 5 6";
        OIIO_CHECK_ASSERT(!parse_values(sv, "xxx", xyz, ","));
        sv = "xxx 1, 2.5,3, 4, 5,6";
        OIIO_CHECK_ASSERT(parse_values(sv, "xxx", xyz, ",")
                          && xyz[0] == 1 && xyz[1] == 2.5 && xyz[2] == 3
                          && sv == ", 4, 5,6");
    }

    string_view ss;
    s = "foo bar";
    OIIO_CHECK_ASSERT (parse_string (s, ss) && ss == "foo" && s == " bar");
    s = "\"foo bar\" baz";
    OIIO_CHECK_ASSERT (parse_string (s, ss) && ss == "foo bar" && s == " baz");
    s = "\"foo bar\" baz";
    OIIO_CHECK_ASSERT (parse_string (s, ss, false) && ss == "foo bar" && s == "\"foo bar\" baz");
    s = "\"foo bar\" baz";
    parse_string (s, ss, true, KeepQuotes);
    OIIO_CHECK_EQUAL (ss, "\"foo bar\"");
    OIIO_CHECK_EQUAL (s, " baz");
    s = "\"foo bar\" baz";
    parse_string (s, ss, true, DeleteQuotes);
    OIIO_CHECK_EQUAL (ss, "foo bar");
    OIIO_CHECK_EQUAL (s, " baz");
    s = "'foo bar' baz";

    s = "\"foo \\\"bar\\\" baz\" blort";
    parse_string (s, ss, true, DeleteQuotes);
    OIIO_CHECK_EQUAL (ss, "foo \\\"bar\\\" baz");
    OIIO_CHECK_EQUAL (s, " blort");
    s = "\"foo \\\"bar\\\" baz\" blort";
    parse_string (s, ss, true, KeepQuotes);
    OIIO_CHECK_EQUAL (ss, "\"foo \\\"bar\\\" baz\"");
    OIIO_CHECK_EQUAL (s, " blort");

    s = "'foo bar' baz";
    parse_string (s, ss, true, KeepQuotes);
    OIIO_CHECK_EQUAL (ss, "'foo bar'");
    OIIO_CHECK_EQUAL (s, " baz");
    s = "'foo bar' baz";
    parse_string (s, ss, true, DeleteQuotes);
    OIIO_CHECK_EQUAL (ss, "foo bar");
    OIIO_CHECK_EQUAL (s, " baz");

    s = " foo bar"; ss = parse_word (s);
    OIIO_CHECK_ASSERT (ss == "foo" && s == " bar");
    s = " 14 foo bar"; ss = parse_word (s);
    OIIO_CHECK_ASSERT (ss.size() == 0 && s == " 14 foo bar");
    s = "foo14 bar"; ss = parse_word (s);
    OIIO_CHECK_ASSERT (ss == "foo" && s == "14 bar");
    s = " foo bar"; ss = parse_word (s, false);
    OIIO_CHECK_ASSERT (ss == "foo" && s == " foo bar");

    s = " foo bar"; ss = parse_identifier (s);
    OIIO_CHECK_ASSERT (ss == "foo" && s == " bar");
    s = " 14 foo bar"; ss = parse_identifier (s);
    OIIO_CHECK_ASSERT (ss.size() == 0 && s == " 14 foo bar");
    s = " foo_14 bar"; ss = parse_identifier (s);
    OIIO_CHECK_ASSERT (ss == "foo_14" && s == " bar");
    s = " foo_14 bar"; ss = parse_identifier (s, false);
    OIIO_CHECK_ASSERT (ss == "foo_14" && s == " foo_14 bar");
    s = "fl$orp 14";  ss = parse_identifier (s);
    OIIO_CHECK_ASSERT (ss == "fl" && s == "$orp 14");
    s = "fl$orp 14";  ss = parse_identifier (s, "$:", true);
    OIIO_CHECK_ASSERT (ss == "fl$orp" && s == " 14");

    bool b;
    s = " foo bar"; b = parse_identifier_if (s, "bar");
    OIIO_CHECK_ASSERT (b == false && s == " foo bar");
    s = " foo bar"; b = parse_identifier_if (s, "foo");
    OIIO_CHECK_ASSERT (b == true && s == " bar");
    s = " foo_14 bar"; b = parse_identifier_if (s, "foo");
    OIIO_CHECK_ASSERT (b == false && s == " foo_14 bar");
    s = " foo_14 bar"; b = parse_identifier_if (s, "foo_14");
    OIIO_CHECK_ASSERT (b == true && s == " bar");

    s = "foo;bar blow"; ss = parse_until (s, ";");
    OIIO_CHECK_ASSERT (ss == "foo" && s == ";bar blow");
    s = "foo;bar blow"; ss = parse_until (s, "\t ");
    OIIO_CHECK_ASSERT (ss == "foo;bar" && s == " blow");
    s = "foo;bar blow"; ss = parse_until (s, "/");
    OIIO_CHECK_ASSERT (ss == "foo;bar blow" && s == "");

    s = "foo;bar blow"; ss = parse_while (s, "of");
    OIIO_CHECK_ASSERT (ss == "foo" && s == ";bar blow");
    s = "foo;bar blow"; ss = parse_while (s, "abc");
    OIIO_CHECK_ASSERT (ss == "" && s == "foo;bar blow");

    s = "first line\nsecond line";
    ss = parse_line(s, false);
    OIIO_CHECK_ASSERT (ss == "first line\n" && s == "first line\nsecond line");
    ss = parse_line(s);
    OIIO_CHECK_ASSERT (ss == "first line\n" && s == "second line");
    ss = parse_line(s);
    OIIO_CHECK_ASSERT (ss == "second line" && s == "");

    s = "[a([b]c)]x]"; ss = parse_nested (s);
    OIIO_CHECK_EQUAL (ss, "[a([b]c)]"); OIIO_CHECK_EQUAL (s, "x]");
    s = "[a([b]c)]x]"; ss = parse_nested (s, false);  // no eating
    OIIO_CHECK_EQUAL (ss, "[a([b]c)]"); OIIO_CHECK_EQUAL (s, "[a([b]c)]x]");
    s = "([a([b]c)])x]"; ss = parse_nested (s);
    OIIO_CHECK_EQUAL (ss, "([a([b]c)])"); OIIO_CHECK_EQUAL (s, "x]");
    s = "blah[a([b]c)]x]"; ss = parse_nested (s);
    OIIO_CHECK_EQUAL (ss, ""); OIIO_CHECK_EQUAL (s, "blah[a([b]c)]x]");
    s = ""; ss = parse_nested (s);
    OIIO_CHECK_EQUAL (ss, ""); OIIO_CHECK_EQUAL (s, "");
    s = "(blah"; ss = parse_nested (s);
    OIIO_CHECK_EQUAL (ss, ""); OIIO_CHECK_EQUAL (s, "(blah");

    OIIO_CHECK_EQUAL(string_is_identifier("valid"), true);
    OIIO_CHECK_EQUAL(string_is_identifier("_underscore"), true);
    OIIO_CHECK_EQUAL(string_is_identifier("with123numbers"), true);
    OIIO_CHECK_EQUAL(string_is_identifier("123invalidStart"), false);
    OIIO_CHECK_EQUAL(string_is_identifier("invalid-char"), false);
    OIIO_CHECK_EQUAL(string_is_identifier(""), false);
    OIIO_CHECK_EQUAL(string_is_identifier("a"), true);
    OIIO_CHECK_EQUAL(string_is_identifier("_"), true);
    OIIO_CHECK_EQUAL(string_is_identifier("1"), false);
}



void
test_locale()
{
    std::cout << "Testing float conversion + locale\n";
    std::locale oldloc
        = std::locale::global(std::locale::classic());  // save original locale
    try {
        // Just in case we're on a system that can't do this?
        std::locale::global(std::locale("fr_FR.UTF-8"));
        const char* numcstr = "123.45";
        std::string numstring(numcstr);
        std::cout << "safe float convert (C locale) " << numcstr << " = "
                  << Strutil::stof(numcstr) << "\n";
        OIIO_CHECK_EQUAL_APPROX(Strutil::stof(numcstr), 123.45f);
        std::cout << "unsafe float convert (default locale) " << numcstr << " = "
                  << atof(numcstr) << "\n"; // NOLINT(cert-err34-c)
        OIIO_CHECK_EQUAL_APPROX(atof(numcstr), 123.0f); // NOLINT(cert-err34-c)

        // Verify that Strutil::sprintf does the right thing, even when in a
        // comma-based locale.
        OIIO_CHECK_EQUAL(Strutil::sprintf("%g", 123.45f), "123.45");
        OIIO_CHECK_EQUAL(Strutil::sprintf("%d", 12345), "12345");
#if 0
        // Verify that Strutil::fmt::format does the right thing, even when in a
        // comma-based locale.
        // FIXME: currently broken! Re-enable after {fmt} fixes its ability
        // to format floating point numbers locale-independently.
        OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", 123.45f), "123.45");
        OIIO_CHECK_EQUAL(Strutil::fmt::format("{:.3f}", 123.45f), "123.450");
        OIIO_CHECK_EQUAL(Strutil::fmt::format("{:g}", 123.45f), "123.45");
        OIIO_CHECK_EQUAL(Strutil::fmt::format("{}", 12345), "12345");
        // Verify that fmt::format does use locale when {:n}
        OIIO_CHECK_EQUAL(Strutil::fmt::format("{:g}", 123.45f), "123,45");
        OIIO_CHECK_EQUAL(Strutil::fmt::format("{:n}", 12345), "12,345");
#endif
    std::locale::global(oldloc);  // restore
    } catch (...) {
    }
}



void
test_float_formatting()
{
    // For every possible float value, test that printf("%.9g"), which
    // we are sure preserves full precision as text, exactly matches
    // Strutil::sprintf("%.9g") and also matches stream output with
    // precision(9).  VERY EXPENSIVE!  Takes tens of minutes to run.
    // Don't do this unless you really need to test it.
    for (unsigned long long i = 0; i <= (unsigned long long)0xffffffff; ++i) {
        unsigned int i32 = (unsigned int)i;
        float* f         = (float*)&i32;
        std::ostringstream sstream;
        sstream.precision(9);
        sstream << *f;
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.9g", *f);
        std::string tiny = Strutil::sprintf("%.9g", *f);
        if (sstream.str() != tiny || tiny != buffer)
            Strutil::printf(
                "%x  stream '%s'  printf '%s'  Strutil::sprintf '%s'\n", i32,
                sstream.str(), buffer, tiny);
        if ((i32 & 0xfffffff) == 0xfffffff) {
            Strutil::printf("%x\n", i32);
            fflush(stdout);
        }
    }
}



template<typename S, typename T>
void
test_string_compare_function_impl()
{
    S foo("foo");
    // Test same string
    OIIO_CHECK_EQUAL(foo.compare(T("foo")), 0);
    // Test different string of same length
    OIIO_CHECK_GE(foo.compare(T("bar")), 0);
    OIIO_CHECK_GE(foo.compare(T("fon")), 0);
    OIIO_CHECK_LE(foo.compare(T("fop")), 0);
    // Test against shorter
    OIIO_CHECK_GE(foo.compare(T("a")), 0);
    OIIO_CHECK_GE(foo.compare(T("fo")), 0); // common sub, ing
    OIIO_CHECK_LE(foo.compare(T("foobar")), 0); // common substring
    OIIO_CHECK_GE(foo.compare(T("bart")), 0);
    // Test against empty string
    OIIO_CHECK_GE(foo.compare(""), 0);
}


void
test_string_compare_function()
{
    test_string_compare_function_impl<ustring, const char*>();
    test_string_compare_function_impl<ustring, string_view>();
    test_string_compare_function_impl<ustring, ustring>();
    test_string_compare_function_impl<ustring, std::string>();

    test_string_compare_function_impl<string_view, const char*>();
    test_string_compare_function_impl<string_view, string_view>();
    test_string_compare_function_impl<string_view, ustring>();
    test_string_compare_function_impl<string_view, std::string>();
}



void
test_datetime()
{
    using namespace Strutil;
    int y = -1, m = -1, d = -1, h = -1, min = -1, s = -1;

    y = -1, m = -1, d = -1, h = -1, min = -1, s = -1;
    OIIO_CHECK_ASSERT(scan_datetime("2020-05-01 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(y == 2020 && m == 5 && d == 1 && h == 12 && min == 34 && s == 21);

    y = -1, m = -1, d = -1, h = -1, min = -1, s = -1;
    OIIO_CHECK_ASSERT(scan_datetime("2020/05/01 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(y == 2020 && m == 5 && d == 1 && h == 12 && min == 34 && s == 21);

    y = -1, m = -1, d = -1, h = -1, min = -1, s = -1;
    OIIO_CHECK_ASSERT(scan_datetime("2020:05:01 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(y == 2020 && m == 5 && d == 1 && h == 12 && min == 34 && s == 21);

    y = -1, m = -1, d = -1, h = -1, min = -1, s = -1;
    OIIO_CHECK_ASSERT(scan_datetime("2020:05:01 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(y == 2020 && m == 5 && d == 1 && h == 12 && min == 34 && s == 21);

    // No time
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:01", y, m, d, h, min, s));
    // Out of range values
    OIIO_CHECK_ASSERT(!scan_datetime("2020:00:01 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:13:01 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:00 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:32 12:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:01 24:34:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:01 24:60:21", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:01 12:34:60", y, m, d, h, min, s));
    OIIO_CHECK_ASSERT(!scan_datetime("2020:05:01 12:34:-1", y, m, d, h, min, s));
}



void
test_edit_distance()
{
    using namespace Strutil;
    print("test_edit_distance\n");
    OIIO_CHECK_EQUAL(edit_distance("", ""), 0);
    OIIO_CHECK_EQUAL(edit_distance("", "abc"), 3);
    OIIO_CHECK_EQUAL(edit_distance("abcd", ""), 4);
    OIIO_CHECK_EQUAL(edit_distance("abc", "abc"), 0);
    OIIO_CHECK_EQUAL(edit_distance("abc", "ab"), 1);
    OIIO_CHECK_EQUAL(edit_distance("abc", "abcde"), 2);
    OIIO_CHECK_EQUAL(edit_distance("abc", "abd"), 1);
    OIIO_CHECK_EQUAL(edit_distance("sitting", "kitten"), 3);
}



void
test_base64_encode()
{
    OIIO_CHECK_EQUAL(Strutil::base64_encode("foo123,()"), "Zm9vMTIzLCgp");
}



void
test_eval_as_bool()
{
    using namespace Strutil;
    print("testing eval_as_bool()\n");

    // Test cases for integer values
    OIIO_CHECK_EQUAL(eval_as_bool("0"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("1"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("-1"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("10"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("-10"), true);

    // Test cases for floating-point values
    OIIO_CHECK_EQUAL(eval_as_bool("0.0"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("1.0"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("-1.0"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("10.5"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("-10.5"), true);

    // Test cases for string values
    OIIO_CHECK_EQUAL(eval_as_bool(""), false);
    OIIO_CHECK_EQUAL(eval_as_bool("false"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("FALSE"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("no"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("NO"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("off"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("OFF"), false);

    OIIO_CHECK_EQUAL(eval_as_bool("true"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("TRUE"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("yes"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("YES"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("on"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("ON"), true);
    OIIO_CHECK_EQUAL(eval_as_bool("OpenImageIO"), true);

    // Test whitespace, case insensitivity, other tricky cases
    OIIO_CHECK_EQUAL(eval_as_bool("   "), false);
    OIIO_CHECK_EQUAL(eval_as_bool("\t \n"), false);
    OIIO_CHECK_EQUAL(eval_as_bool(" faLsE"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("\tOFf"), false);
    OIIO_CHECK_EQUAL(eval_as_bool("off OpenImageIO"), true);
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_format();
    test_format_custom();
    test_memformat();
    test_timeintervalformat();
    test_get_rest_arguments();
    test_escape_sequences();
    test_wordwrap();
    test_hash();
    test_comparisons();
    test_case();
    test_strip();
    test_splits();
    test_splitsv();
    test_join();
    test_concat();
    test_repeat();
    test_replace();
    test_excise_string_after_head();
    test_numeric_conversion();
    test_to_string();
    test_extract();
    test_safe_strcpy();
    test_safe_strcat();
    test_safe_strlen();
    test_string_view();
    test_parse();
    test_locale();
    // test_float_formatting ();
    test_string_compare_function();
    test_datetime();
    test_edit_distance();
    test_base64_encode();
    test_eval_as_bool();

    Strutil::debug("debug message\n");

    return unit_test_failures;
}
