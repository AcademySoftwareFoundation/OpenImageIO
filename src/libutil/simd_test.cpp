// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

#include <sstream>
#include <type_traits>

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/ustring.h>



using namespace OIIO;

using namespace OIIO::simd;


static int iterations = 1000000;
static int ntrials    = 5;
static Sysutil::Term term(std::cout);
OIIO_SIMD16_ALIGN float dummy_float[16];
OIIO_SIMD16_ALIGN float dummy_float2[16];
OIIO_SIMD16_ALIGN float dummy_int[16];



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    ap.intro("simd_test -- unit test and benchmarks for OpenImageIO/simd.h\n"
             OIIO_INTRO_STRING)
      .usage("simd_test [options]");

    ap.arg("--iterations %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");

    ap.parse_args(argc, (const char**)argv);
}



static void
category_heading(string_view name)
{
    std::cout << "\n" << term.ansi("bold,underscore,yellow", name) << "\n\n";
}



static void
test_heading(string_view name, string_view name2 = "")
{
    std::cout << term.ansi("bold") << name << ' ' << name2
              << term.ansi("normal") << "\n";
}



// What I really want to do is merge benchmark() and benchmark2() into
// one template using variadic arguments, like this:
//   template <typename FUNC, typename ...ARGS>
//   void benchmark (size_t work, string_view funcname, FUNC func, ARGS... args)
// But it seems that although this works for Clang, it does not for gcc 4.8
// (but does for 4.9). Some day I'll get back to this simplification, but
// for now, gcc 4.8 seems like an important barrier.


template<typename FUNC, typename T>
void
benchmark(string_view funcname, FUNC func, T x, size_t work = 0)
{
    if (!work)
        work = SimdElements<decltype(func(x))>::size;
    auto repeat_func = [&](){
        // Unroll the loop 8 times
        auto r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
    };
    float time = time_trial(repeat_func, ntrials, iterations / 8);
    Strutil::print("  {}: {:7.1f} Mvals/sec, ({:.1f} Mcalls/sec)\n",
                                 funcname, ((iterations * work) / 1.0e6) / time,
                                 (iterations / 1.0e6) / time);
}


template<typename FUNC, typename T, typename U>
void
benchmark2(string_view funcname, FUNC func, T x, U y, size_t work = 0)
{
    if (!work)
        work = SimdElements<decltype(func(x, y))>::size;
    auto repeat_func = [&]() {
        // Unroll the loop 8 times
        auto r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
    };
    float time = time_trial(repeat_func, ntrials, iterations / 8);
    Strutil::print("  {}: {:7.1f} Mvals/sec, ({:.1f} Mcalls/sec)\n",
                                 funcname, ((iterations * work) / 1.0e6) / time,
                                 (iterations / 1.0e6) / time);
}



template<typename VEC>
inline VEC
mkvec(typename VEC::value_t a, typename VEC::value_t b, typename VEC::value_t c,
      typename VEC::value_t d = 0)
{
    return VEC(a, b, c, d);
}

template<>
inline vfloat3
mkvec<vfloat3>(float a, float b, float c, float /*d*/)
{
    return vfloat3(a, b, c);
}

template<>
inline vfloat8
mkvec<vfloat8>(float a, float b, float c, float d)
{
    return vfloat8(a, b, c, d, a, b, c, d);
}

template<>
inline vfloat16
mkvec<vfloat16>(float a, float b, float c, float d)
{
    return vfloat16(a, b, c, d, a, b, c, d, a, b, c, d, a, b, c, d);
}

template<>
inline vint8
mkvec<vint8>(int a, int b, int c, int d)
{
    return vint8(a, b, c, d, a, b, c, d);
}

template<>
inline vint16
mkvec<vint16>(int a, int b, int c, int d)
{
    return vint16(a, b, c, d, a, b, c, d, a, b, c, d, a, b, c, d);
}

template<>
inline vbool8
mkvec<vbool8>(bool a, bool b, bool c, bool d)
{
    return vbool8(a, b, c, d, a, b, c, d);
}

template<>
inline vbool16
mkvec<vbool16>(bool a, bool b, bool c, bool d)
{
    return vbool16(a, b, c, d, a, b, c, d, a, b, c, d, a, b, c, d);
}



template<typename VEC>
inline VEC
mkvec(typename VEC::value_t a, typename VEC::value_t b, typename VEC::value_t c,
      typename VEC::value_t d, typename VEC::value_t e, typename VEC::value_t f,
      typename VEC::value_t g, typename VEC::value_t h)
{
    return VEC(a, b, c, d, e, f, g, h);
}


template<>
inline vbool4
mkvec<vbool4>(bool a, bool b, bool c, bool d, bool, bool, bool, bool)
{
    return vbool4(a, b, c, d);
}

template<>
inline vint4
mkvec<vint4>(int a, int b, int c, int d, int, int, int, int)
{
    return vint4(a, b, c, d);
}

template<>
inline vint16
mkvec<vint16>(int a, int b, int c, int d, int e, int f, int g, int h)
{
    return vint16(a, b, c, d, e, f, g, h, h + 1, h + 2, h + 3, h + 4, h + 5,
                  h + 6, h + 7, h + 8);
}

template<>
inline vfloat4
mkvec<vfloat4>(float a, float b, float c, float d, float, float, float, float)
{
    return vfloat4(a, b, c, d);
}

template<>
inline vfloat3
mkvec<vfloat3>(float a, float b, float c, float, float, float, float, float)
{
    return vfloat3(a, b, c);
}

template<>
inline vfloat16
mkvec<vfloat16>(float a, float b, float c, float d, float e, float f, float g,
                float h)
{
    return vfloat16(a, b, c, d, e, f, g, h, h + 1, h + 2, h + 3, h + 4, h + 5,
                    h + 6, h + 7, h + 8);
}



template<typename VEC>
inline int
loadstore_vec(int /*dummy*/)
{
    typedef typename VEC::value_t ELEM;
    ELEM B[VEC::elements];
    VEC v;
    v.load((ELEM*)dummy_float);
    DoNotOptimize(v);
    clobber_all_memory();
    v.store((ELEM*)B);
    DoNotOptimize(B[0]);
    return 0;
}

template<typename VEC>
inline VEC
load_vec(int /*dummy*/)
{
    typedef typename VEC::value_t ELEM;
    VEC v;
    v.load((ELEM*)dummy_float);
    return v;
}

template<typename VEC>
inline int
store_vec(const VEC& v)
{
    typedef typename VEC::value_t ELEM;
    v.store((ELEM*)dummy_float);
    return 0;
}

template<typename VEC>
inline VEC
load_scalar(int /*dummy*/)
{
    typedef typename VEC::value_t ELEM;
    VEC v;
OIIO_PRAGMA_WARNING_PUSH
OIIO_GCC_ONLY_PRAGMA(GCC diagnostic ignored "-Wstrict-aliasing")
    v.load(*(ELEM*)dummy_float);
OIIO_PRAGMA_WARNING_POP
    return v;
}

template<typename VEC, int N>
inline VEC
load_vec_N(typename VEC::value_t* /*B*/)
{
    typedef typename VEC::value_t ELEM;
    VEC v;
    v.load((ELEM*)dummy_float, N);
    return v;
}

template<typename VEC, int N>
inline int
store_vec_N(const VEC& v)
{
    typedef typename VEC::value_t ELEM;
    v.store((ELEM*)dummy_float, N);
    DoNotOptimize(dummy_float[0]);
    return 0;
}



inline float
dot_imath(const Imath::V3f& v)
{
    return v.dot(v);
}
inline float
dot_imath_simd(const Imath::V3f& v_)
{
    vfloat3 v(v_);
    return simd::dot(v, v);
}
inline float
dot_simd(const simd::vfloat3& v)
{
    return dot(v, v);
}

inline Imath::V3f
norm_imath(const Imath::V3f& a)
{
    return a.normalized();
}

inline Imath::V3f
norm_imath_simd(const vfloat3& a)
{
    return a.normalized().V3f();
}

inline Imath::V3f
norm_imath_simd_fast(const vfloat3& a)
{
    return a.normalized_fast().V3f();
}

inline vfloat3
norm_simd_fast(const vfloat3& a)
{
    return a.normalized_fast();
}

inline vfloat3
norm_simd(const vfloat3& a)
{
    return a.normalized();
}


inline Imath::M44f
inverse_imath(const Imath::M44f& M)
{
    return M.inverse();
}


inline matrix44
inverse_simd(const matrix44& M)
{
    return M.inverse();
}



template<typename VEC>
void
test_loadstore()
{
    typedef typename VEC::value_t ELEM;
    test_heading("load/store ", VEC::type_name());
    OIIO_SIMD16_ALIGN ELEM oneval[]
        = { 101, 101, 101, 101, 101, 101, 101, 101,
            101, 101, 101, 101, 101, 101, 101, 101 };
    OIIO_CHECK_SIMD_EQUAL(VEC(oneval), VEC(oneval[0]));
    { VEC a = oneval[0]; OIIO_CHECK_SIMD_EQUAL(VEC(oneval), a); }
    OIIO_SIMD16_ALIGN VEC C1234 = VEC::Iota(1);
    OIIO_SIMD16_ALIGN ELEM partial[]
        = { 101, 102, 103, 104, 105, 106, 107, 108,
            109, 110, 111, 112, 113, 114, 115, 116 };
    OIIO_CHECK_SIMD_EQUAL(VEC(partial), VEC::Iota(101));
    for (int i = 1; i <= VEC::elements; ++i) {
        VEC a(ELEM(0));
        a.load(partial, i);
        for (int j = 0; j < VEC::elements; ++j)
            OIIO_CHECK_EQUAL(a[j], j < i ? partial[j] : ELEM(0));
        std::cout << "  partial load " << i << " : " << a << "\n";
        ELEM stored[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        C1234.store(stored, i);
        for (int j = 0; j < VEC::elements; ++j)
            OIIO_CHECK_EQUAL(stored[j], j < i ? ELEM(j + 1) : ELEM(0));
        std::cout << "  partial store " << i << " :";
        for (int c = 0; c < VEC::elements; ++c)
            std::cout << ' ' << stored[c];
        std::cout << std::endl;
    }

    benchmark("load scalar", load_scalar<VEC>, 0, VEC::elements);
    benchmark("load vec", load_vec<VEC>, 0, VEC::elements);
    benchmark("store vec", store_vec<VEC>, 0, VEC::elements);
    OIIO_SIMD16_ALIGN ELEM tmp[VEC::elements];
    if (VEC::elements == 16) {
        benchmark("load 16 comps", load_vec_N<VEC, 16>, tmp, 16);
        benchmark("load 13 comps", load_vec_N<VEC, 13>, tmp, 13);
        benchmark("load 9 comps", load_vec_N<VEC, 9>, tmp, 9);
    }
    if (VEC::elements > 4) {
        benchmark("load 8 comps", load_vec_N<VEC, 8>, tmp, 8);
        benchmark("load 7 comps", load_vec_N<VEC, 7>, tmp, 7);
        benchmark("load 6 comps", load_vec_N<VEC, 6>, tmp, 6);
        benchmark("load 5 comps", load_vec_N<VEC, 5>, tmp, 5);
    }
    if (VEC::elements >= 4) {
        benchmark("load 4 comps", load_vec_N<VEC, 4>, tmp, 4);
    }
    benchmark("load 3 comps", load_vec_N<VEC, 3>, tmp, 3);
    benchmark("load 2 comps", load_vec_N<VEC, 2>, tmp, 2);
    benchmark("load 1 comps", load_vec_N<VEC, 1>, tmp, 1);

    if (VEC::elements == 16) {
        benchmark("store 16 comps", store_vec_N<VEC, 16>, C1234, 16);
        benchmark("store 13 comps", store_vec_N<VEC, 13>, C1234, 13);
        benchmark("store 9 comps", store_vec_N<VEC, 9>, C1234, 9);
    }
    if (VEC::elements > 4) {
        benchmark("store 8 comps", store_vec_N<VEC, 8>, C1234, 8);
        benchmark("store 7 comps", store_vec_N<VEC, 7>, C1234, 7);
        benchmark("store 6 comps", store_vec_N<VEC, 6>, C1234, 6);
        benchmark("store 5 comps", store_vec_N<VEC, 5>, C1234, 5);
    }
    if (VEC::elements >= 4) {
        benchmark("store 4 comps", store_vec_N<VEC, 4>, C1234, 4);
    }
    benchmark("store 3 comps", store_vec_N<VEC, 3>, C1234, 3);
    benchmark("store 2 comps", store_vec_N<VEC, 2>, C1234, 2);
    benchmark("store 1 comps", store_vec_N<VEC, 1>, C1234, 1);
}



template<typename VEC>
void
test_conversion_loadstore_float()
{
    typedef typename VEC::value_t ELEM;
    test_heading("load/store with conversion", VEC::type_name());
    VEC C1234      = VEC::Iota(1);
    ELEM partial[] = { 101, 102, 103, 104, 105, 106, 107, 108,
                       109, 110, 111, 112, 113, 114, 115, 116 };
    OIIO_CHECK_SIMD_EQUAL(VEC(partial), VEC::Iota(101));

    // Check load from integers
    unsigned short us1234[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    short s1234[]           = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char uc1234[]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    char c1234[]            = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    half h1234[]            = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    OIIO_CHECK_SIMD_EQUAL (VEC(us1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( s1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(uc1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( c1234), C1234);

    benchmark ("load from unsigned short[]", [](const unsigned short *d){ return VEC(d); }, us1234);
    benchmark ("load from short[]", [](const short *d){ return VEC(d); }, s1234);
    benchmark ("load from unsigned char[]", [](const unsigned char *d){ return VEC(d); }, uc1234);
    benchmark ("load from char[]", [](const char *d){ return VEC(d); }, c1234);
    benchmark ("load from half[]", [](const half *d){ return VEC(d); }, h1234);

    benchmark ("store to half[]", [=](half *d){ C1234.store(d); return 0; }, h1234, VEC::elements);
}



template<typename VEC>
void test_conversion_loadstore_int ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("load/store with conversion", VEC::type_name());
    VEC C1234 = VEC::Iota(1);
    ELEM partial[] = { 101, 102, 103, 104, 105, 106, 107, 108,
                       109, 110, 111, 112, 113, 114, 115, 116 };
    OIIO_CHECK_SIMD_EQUAL (VEC(partial), VEC::Iota(101));

    // Check load from integers
    int i1234[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned short us1234[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    short s1234[]           = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char uc1234[]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    char c1234[]            = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    OIIO_CHECK_SIMD_EQUAL (VEC( i1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(us1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( s1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(uc1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( c1234), C1234);

    // Check store to integers
    VEC CStep = VEC::Iota(-130, 131);
    unsigned char ucStepExp[]  = {126, 1, 132, 7, 138, 13, 144, 19, 150, 25, 156, 31, 162, 37, 168, 43};
    unsigned char ucStepGot[VEC::elements] = {};
    CStep.store(ucStepGot);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL ((int)ucStepGot[i], (int)ucStepExp[i]);

    benchmark ("load from int[]", [](const int *d){ return VEC(d); }, i1234);
    benchmark ("load from unsigned short[]", [](const unsigned short *d){ return VEC(d); }, us1234);
    benchmark ("load from short[]", [](const short *d){ return VEC(d); }, s1234);
    benchmark ("load from unsigned char[]", [](const unsigned char *d){ return VEC(d); }, uc1234);
    benchmark ("load from char[]", [](const char *d){ return VEC(d); }, c1234);

    benchmark ("store to unsigned short[]", [=](unsigned short *d){ C1234.store(d); return 0; }, us1234, VEC::elements);
    benchmark ("store to unsigned char[]", [=](unsigned char *d){ C1234.store(d); return 0; }, uc1234, VEC::elements);
}



template<typename VEC>
void test_vint_to_uint16s ()
{
    test_heading (Strutil::fmt::format("test converting {} to uint16",
                                       VEC::type_name()));
    VEC ival = VEC::Iota (0xffff0000);
    unsigned short buf[VEC::elements];
    ival.store (buf);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (int(buf[i]), i);

    benchmark2 ("load from uint16", [](VEC& a, unsigned short *s){ a.load(s); return 1; }, ival, buf, VEC::elements);
    benchmark2 ("convert to uint16", [](const VEC& a, unsigned short *s){ a.store(s); return 1; }, ival, buf, VEC::elements);
}



template<typename VEC>
void test_vint_to_uint8s ()
{
    test_heading (Strutil::fmt::format("test converting {} to uint8",
                                       VEC::type_name()));
    VEC ival = VEC::Iota (0xffffff00);
    unsigned char buf[VEC::elements];
    ival.store (buf);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (int(buf[i]), i);

    benchmark2 ("load from uint8", [](VEC& a, unsigned char *s){ a.load(s); return 1; }, ival, buf, VEC::elements);
    benchmark2 ("convert to uint16", [](const VEC& a, unsigned char *s){ a.store(s); return 1; }, ival, buf, VEC::elements);
}



template<typename VEC>
void test_masked_loadstore ()
{
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::vbool_t BOOL;
    test_heading ("masked loadstore ", VEC::type_name());
    ELEM iota[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    BOOL mask1 = mkvec<BOOL> (true, false, true, false);
    BOOL mask2 = mkvec<BOOL> (true, true, false,false);

    VEC v;
    v = -1;
    v.load_mask (mask1, iota);
    ELEM r1[] = { 1, 0, 3, 0, 5, 0, 7, 0, 9, 0, 11, 0, 13, 0, 15, 0 };
    OIIO_CHECK_SIMD_EQUAL (v, VEC(r1));
    ELEM buf[] = { -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2 };
    v.store_mask (mask2, buf);
    ELEM r2[] = { 1, 0, -2, -2, 5, 0, -2, -2, 9, 0, -2, -2, 13, 0, -2, -2 };
    OIIO_CHECK_SIMD_EQUAL (VEC(buf), VEC(r2));

    benchmark ("masked load with int mask", [](const ELEM *d){ VEC v; v.load_mask (0xffff, d); return v; }, iota);
    benchmark ("masked load with bool mask", [](const ELEM *d){ VEC v; v.load_mask (BOOL::True(), d); return v; }, iota);
    benchmark ("masked store with int mask", [&](ELEM *d){ v.store_mask (0xffff, d); return 0; }, r2);
    benchmark ("masked store with bool mask", [&](ELEM *d){ v.store_mask (BOOL::True(), d); return 0; }, r2);
}



template<typename VEC>
void
test_gatherscatter()
{
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::vbool_t BOOL;
    test_heading("scatter & gather ", VEC::type_name());

    const int spacing = 3;
    const int bufsize = VEC::elements * 3 + 1;
    std::vector<ELEM> gather_source(bufsize);
    for (int i = 0; i < bufsize; ++i)
        gather_source[i] = ((i % spacing) == 1) ? i / 3 : -1;
    // gather_source will contain: -1 0 -1  -1 1 -1  -1 2 -1  -1 3 -1  ...

    auto indices = VEC::vint_t::Iota(1, 3);
    VEC g, gm;
    g.gather(gather_source.data(), indices);
    OIIO_CHECK_SIMD_EQUAL(g, VEC::Iota());

    BOOL mask = BOOL::from_bitmask(0x55555555);  // every other one
    gm = 42;
    gm.gather_mask (mask, gather_source.data(), indices);
    ELEM every_other_iota[] = { 0, 42, 2, 42, 4, 42, 6, 42, 8, 42, 10, 42, 12, 42, 14, 42 };
    OIIO_CHECK_SIMD_EQUAL (gm, VEC(every_other_iota));

    std::vector<ELEM> scatter_out (bufsize, (ELEM)-1);
    g.scatter (scatter_out.data(), indices);
    OIIO_CHECK_ASSERT (scatter_out == gather_source);

    std::fill (scatter_out.begin(), scatter_out.end(), -1);
    VEC::Iota().scatter_mask (mask, scatter_out.data(), indices);
    for (int i = 0; i < (int)scatter_out.size(); ++i)
        OIIO_CHECK_EQUAL (scatter_out[i], ((i%3) == 1 && (i&1) ? i/3 : -1));

    benchmark ("gather", [&](const ELEM *d){ VEC v; v.gather (d, indices); return v; }, gather_source.data());
    benchmark ("gather_mask", [&](const ELEM *d){ VEC v = ELEM(0); v.gather_mask (mask, d, indices); return v; }, gather_source.data());
    benchmark ("scatter", [&](ELEM *d){ g.scatter (d, indices); return g; }, scatter_out.data());
    benchmark ("scatter_mask", [&](ELEM *d){ g.scatter_mask (mask, d, indices); return g; }, scatter_out.data());
}



template<typename T>
void test_extract3 ()
{
    const T vals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    using VEC = typename VecType<T,3>::type;
    VEC b (vals);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (b[i], vals[i]);
    OIIO_CHECK_EQUAL (extract<0>(b), 0);
    OIIO_CHECK_EQUAL (extract<1>(b), 1);
    OIIO_CHECK_EQUAL (extract<2>(b), 2);
}

template<typename T>
void
test_extract4()
{
    const T vals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    using VEC      = typename VecType<T, 4>::type;
    VEC b(vals);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL(b[i], vals[i]);
    OIIO_CHECK_EQUAL(extract<0>(b), 0);
    OIIO_CHECK_EQUAL(extract<1>(b), 1);
    OIIO_CHECK_EQUAL(extract<2>(b), 2);
    OIIO_CHECK_EQUAL(extract<3>(b), 3);
}

template<typename T>
void
test_extract8()
{
    test_extract4<T>();

    const T vals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    using VEC      = typename VecType<T, 8>::type;
    VEC b(vals);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL(b[i], vals[i]);
    OIIO_CHECK_EQUAL(extract<4>(b), 4);
    OIIO_CHECK_EQUAL(extract<5>(b), 5);
    OIIO_CHECK_EQUAL(extract<6>(b), 6);
    OIIO_CHECK_EQUAL(extract<7>(b), 7);
}

template<typename T>
void
test_extract16()
{
    test_extract8<T>();

    const T vals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    using VEC      = typename VecType<T, 16>::type;
    VEC b(vals);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL(b[i], vals[i]);
    OIIO_CHECK_EQUAL(extract<8>(b), 8);
    OIIO_CHECK_EQUAL(extract<9>(b), 9);
    OIIO_CHECK_EQUAL(extract<10>(b), 10);
    OIIO_CHECK_EQUAL(extract<11>(b), 11);
    OIIO_CHECK_EQUAL(extract<12>(b), 12);
    OIIO_CHECK_EQUAL(extract<13>(b), 13);
    OIIO_CHECK_EQUAL(extract<14>(b), 14);
    OIIO_CHECK_EQUAL(extract<15>(b), 15);
}



template<typename T, int SIZE> void test_extract ();
template<> void test_extract<float,16> () { test_extract16<float>(); }
template<> void test_extract<int,16> () { test_extract16<int>(); }
template<> void test_extract<float,8> () { test_extract8<float>(); }
template<> void test_extract<int,8> () { test_extract8<int>(); }
template<> void test_extract<float,4> () { test_extract4<float>(); }
template<> void test_extract<int,4> () { test_extract4<int>(); }
template<> void test_extract<float,3> () { test_extract3<float>(); }



template<typename VEC>
void
test_component_access()
{
    typedef typename VEC::value_t ELEM;
    test_heading("component_access ", VEC::type_name());

    const ELEM vals[]
        = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    VEC a = VEC::Iota();
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL(a[i], vals[i]);

    if (VEC::elements <= 4) {
        OIIO_CHECK_EQUAL(a.x(), 0);
        OIIO_CHECK_EQUAL(a.y(), 1);
        OIIO_CHECK_EQUAL(a.z(), 2);
        if (SimdElements<VEC>::size > 3)
            OIIO_CHECK_EQUAL(a.w(), 3);
        VEC t;
        t = a;
        t.set_x(42);
        OIIO_CHECK_SIMD_EQUAL(t, mkvec<VEC>(42, 1, 2, 3, 4, 5, 6, 7));
        t = a;
        t.set_y(42);
        OIIO_CHECK_SIMD_EQUAL(t, mkvec<VEC>(0, 42, 2, 3, 4, 5, 6, 7));
        t = a;
        t.set_z(42);
        OIIO_CHECK_SIMD_EQUAL(t, mkvec<VEC>(0, 1, 42, 3, 4, 5, 6, 7));
        if (SimdElements<VEC>::size > 3) {
            t = a;
            t.set_w(42);
            OIIO_CHECK_SIMD_EQUAL(t, mkvec<VEC>(0, 1, 2, 42, 4, 5, 6, 7));
        }
    }

    OIIO_CHECK_EQUAL(extract<0>(a), 0);
    OIIO_CHECK_EQUAL(extract<1>(a), 1);
    OIIO_CHECK_EQUAL(extract<2>(a), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (extract<3>(a), 3);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, ELEM(42)), mkvec<VEC>(42,1,2,3,4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, ELEM(42)), mkvec<VEC>(0,42,2,3,4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, ELEM(42)), mkvec<VEC>(0,1,42,3,4,5,6,7));
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_SIMD_EQUAL (insert<3>(a, ELEM(42)), mkvec<VEC>(0,1,2,42,4,5,6,7));

    VEC b(vals);
#if 1
    test_extract<ELEM, VEC::elements>();
#else
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL(b[i], vals[i]);
    OIIO_CHECK_EQUAL(extract<0>(b), 0);
    OIIO_CHECK_EQUAL(extract<1>(b), 1);
    OIIO_CHECK_EQUAL(extract<2>(b), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL(extract<3>(b), 3);
    if (SimdElements<VEC>::size > 4) {
        OIIO_CHECK_EQUAL(extract<4>(b), 4);
        OIIO_CHECK_EQUAL(extract<5>(b), 5);
        OIIO_CHECK_EQUAL(extract<6>(b), 6);
        OIIO_CHECK_EQUAL(extract<7>(b), 7);
    }
    if (SimdElements<VEC>::size > 8) {
        OIIO_CHECK_EQUAL(extract<8>(b), 8);
        OIIO_CHECK_EQUAL(extract<9>(b), 9);
        OIIO_CHECK_EQUAL(extract<10>(b), 10);
        OIIO_CHECK_EQUAL(extract<11>(b), 11);
        OIIO_CHECK_EQUAL(extract<12>(b), 12);
        OIIO_CHECK_EQUAL(extract<13>(b), 13);
        OIIO_CHECK_EQUAL(extract<14>(b), 14);
        OIIO_CHECK_EQUAL(extract<15>(b), 15);
    }
#endif

    benchmark2 ("operator[i]", [&](const VEC& v, int i){ return v[i]; },  b, 2, 1 /*work*/);
    benchmark2 ("operator[2]", [&](const VEC& v, int /*i*/){ return v[2]; },  b, 2, 1 /*work*/);
    benchmark2 ("operator[0]", [&](const VEC& v, int /*i*/){ return v[0]; },  b, 0, 1 /*work*/);
    benchmark2 ("extract<2> ", [&](const VEC& v, int /*i*/){ return extract<2>(v); },  b, 2, 1 /*work*/);
    benchmark2 ("extract<0> ", [&](const VEC& v, int /*i*/){ return extract<0>(v); },  b, 0, 1 /*work*/);
    benchmark2 ("insert<2> ", [&](const VEC& v, ELEM i){ return insert<2>(v, i); }, b, ELEM(1), 1 /*work*/);
}



template<>
void
test_component_access<vbool4>()
{
    typedef vbool4 VEC;
    test_heading("component_access ", VEC::type_name());

    for (int bit = 0; bit < VEC::elements; ++bit) {
        VEC ctr(bit == 0, bit == 1, bit == 2, bit == 3);
        VEC a;
        a.clear();
        for (int b = 0; b < VEC::elements; ++b)
            a.setcomp(b, b == bit);
        OIIO_CHECK_SIMD_EQUAL(ctr, a);
        for (int b = 0; b < VEC::elements; ++b)
            OIIO_CHECK_EQUAL(bool(a[b]), b == bit);
        OIIO_CHECK_EQUAL(extract<0>(a), bit == 0);
        OIIO_CHECK_EQUAL(extract<1>(a), bit == 1);
        OIIO_CHECK_EQUAL(extract<2>(a), bit == 2);
        OIIO_CHECK_EQUAL(extract<3>(a), bit == 3);
    }

    VEC a;
    a.load(0, 0, 0, 0);
    OIIO_CHECK_SIMD_EQUAL(insert<0>(a, 1), VEC(1, 0, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<1>(a, 1), VEC(0, 1, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<2>(a, 1), VEC(0, 0, 1, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<3>(a, 1), VEC(0, 0, 0, 1));
    a.load(1, 1, 1, 1);
    OIIO_CHECK_SIMD_EQUAL(insert<0>(a, 0), VEC(0, 1, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<1>(a, 0), VEC(1, 0, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<2>(a, 0), VEC(1, 1, 0, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<3>(a, 0), VEC(1, 1, 1, 0));
}



template<>
void
test_component_access<vbool8>()
{
    typedef vbool8 VEC;
    test_heading("component_access ", VEC::type_name());

    for (int bit = 0; bit < VEC::elements; ++bit) {
        VEC ctr(bit == 0, bit == 1, bit == 2, bit == 3, bit == 4, bit == 5,
                bit == 6, bit == 7);
        VEC a;
        a.clear();
        for (int b = 0; b < VEC::elements; ++b)
            a.setcomp(b, b == bit);
        OIIO_CHECK_SIMD_EQUAL(ctr, a);
        for (int b = 0; b < VEC::elements; ++b)
            OIIO_CHECK_EQUAL(bool(a[b]), b == bit);
        OIIO_CHECK_EQUAL(extract<0>(a), bit == 0);
        OIIO_CHECK_EQUAL(extract<1>(a), bit == 1);
        OIIO_CHECK_EQUAL(extract<2>(a), bit == 2);
        OIIO_CHECK_EQUAL(extract<3>(a), bit == 3);
        OIIO_CHECK_EQUAL(extract<4>(a), bit == 4);
        OIIO_CHECK_EQUAL(extract<5>(a), bit == 5);
        OIIO_CHECK_EQUAL(extract<6>(a), bit == 6);
        OIIO_CHECK_EQUAL(extract<7>(a), bit == 7);
    }

    VEC a;
    a.load(0, 0, 0, 0, 0, 0, 0, 0);
    OIIO_CHECK_SIMD_EQUAL(insert<0>(a, 1), VEC(1, 0, 0, 0, 0, 0, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<1>(a, 1), VEC(0, 1, 0, 0, 0, 0, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<2>(a, 1), VEC(0, 0, 1, 0, 0, 0, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<3>(a, 1), VEC(0, 0, 0, 1, 0, 0, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<4>(a, 1), VEC(0, 0, 0, 0, 1, 0, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<5>(a, 1), VEC(0, 0, 0, 0, 0, 1, 0, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<6>(a, 1), VEC(0, 0, 0, 0, 0, 0, 1, 0));
    OIIO_CHECK_SIMD_EQUAL(insert<7>(a, 1), VEC(0, 0, 0, 0, 0, 0, 0, 1));
    a.load(1, 1, 1, 1, 1, 1, 1, 1);
    OIIO_CHECK_SIMD_EQUAL(insert<0>(a, 0), VEC(0, 1, 1, 1, 1, 1, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<1>(a, 0), VEC(1, 0, 1, 1, 1, 1, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<2>(a, 0), VEC(1, 1, 0, 1, 1, 1, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<3>(a, 0), VEC(1, 1, 1, 0, 1, 1, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<4>(a, 0), VEC(1, 1, 1, 1, 0, 1, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<5>(a, 0), VEC(1, 1, 1, 1, 1, 0, 1, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<6>(a, 0), VEC(1, 1, 1, 1, 1, 1, 0, 1));
    OIIO_CHECK_SIMD_EQUAL(insert<7>(a, 0), VEC(1, 1, 1, 1, 1, 1, 1, 0));
}



template<>
void
test_component_access<vbool16>()
{
    typedef vbool16 VEC;
    test_heading("component_access ", VEC::type_name());

    for (int bit = 0; bit < VEC::elements; ++bit) {
        VEC ctr(bit == 0, bit == 1, bit == 2, bit == 3, bit == 4, bit == 5,
                bit == 6, bit == 7, bit == 8, bit == 9, bit == 10, bit == 11,
                bit == 12, bit == 13, bit == 14, bit == 15);
        VEC a;
        a.clear();
        for (int b = 0; b < VEC::elements; ++b)
            a.setcomp(b, b == bit);
        OIIO_CHECK_SIMD_EQUAL(ctr, a);
        for (int b = 0; b < VEC::elements; ++b)
            OIIO_CHECK_EQUAL(bool(a[b]), b == bit);
        OIIO_CHECK_EQUAL(extract<0>(a), bit == 0);
        OIIO_CHECK_EQUAL(extract<1>(a), bit == 1);
        OIIO_CHECK_EQUAL(extract<2>(a), bit == 2);
        OIIO_CHECK_EQUAL(extract<3>(a), bit == 3);
        OIIO_CHECK_EQUAL(extract<4>(a), bit == 4);
        OIIO_CHECK_EQUAL(extract<5>(a), bit == 5);
        OIIO_CHECK_EQUAL(extract<6>(a), bit == 6);
        OIIO_CHECK_EQUAL(extract<7>(a), bit == 7);
        OIIO_CHECK_EQUAL(extract<8>(a), bit == 8);
        OIIO_CHECK_EQUAL(extract<9>(a), bit == 9);
        OIIO_CHECK_EQUAL(extract<10>(a), bit == 10);
        OIIO_CHECK_EQUAL(extract<11>(a), bit == 11);
        OIIO_CHECK_EQUAL(extract<12>(a), bit == 12);
        OIIO_CHECK_EQUAL(extract<13>(a), bit == 13);
        OIIO_CHECK_EQUAL(extract<14>(a), bit == 14);
        OIIO_CHECK_EQUAL(extract<15>(a), bit == 15);
    }

    VEC a;
    a.load (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    OIIO_CHECK_SIMD_EQUAL (insert<0> (a, 1), VEC(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<1> (a, 1), VEC(0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<2> (a, 1), VEC(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<3> (a, 1), VEC(0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<4> (a, 1), VEC(0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<5> (a, 1), VEC(0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<6> (a, 1), VEC(0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<7> (a, 1), VEC(0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<8> (a, 1), VEC(0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<9> (a, 1), VEC(0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<10>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<11>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<12>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<13>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<14>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0));
    OIIO_CHECK_SIMD_EQUAL (insert<15>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1));
    a.load (1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);
    OIIO_CHECK_SIMD_EQUAL (insert<0> (a, 0), VEC(0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<1> (a, 0), VEC(1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<2> (a, 0), VEC(1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<3> (a, 0), VEC(1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<4> (a, 0), VEC(1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<5> (a, 0), VEC(1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<6> (a, 0), VEC(1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<7> (a, 0), VEC(1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<8> (a, 0), VEC(1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<9> (a, 0), VEC(1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<10>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<11>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<12>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<13>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<14>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1));
    OIIO_CHECK_SIMD_EQUAL (insert<15>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0));
}



template<typename T> inline T do_neg (const T &a) { return -a; }
template<typename T> inline T do_add (const T &a, const T &b) { return a+b; }
template<typename T> inline T do_sub (const T &a, const T &b) { return a-b; }
template<typename T, typename U=T> inline auto do_mul (const T &a, const U &b) -> decltype(a*b) { return a*b; }
template<typename T> inline T do_div (const T &a, const T &b) { return a/b; }
template<typename T> inline T do_safe_div (const T &a, const T &b) { return T(safe_div(a,b)); }
inline Imath::V3f add_vec_simd (const Imath::V3f &a, const Imath::V3f &b) {
    return (vfloat3(a)+vfloat3(b)).V3f();
}
template<typename T> inline T do_abs (const T &a) { return abs(a); }


template<typename VEC>
void test_arithmetic ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("arithmetic ", VEC::type_name());

    ELEM eps = static_cast<ELEM>(1.0e-6);
    VEC a = VEC::Iota (1, 3);
    VEC b = VEC::Iota (1, 1);
    VEC add(ELEM(0)), sub(ELEM(0)), mul(ELEM(0)), div(ELEM(0));
    ELEM bsum(ELEM(0));
    for (int i = 0; i < VEC::elements; ++i) {
        add[i] = a[i] + b[i];
        sub[i] = a[i] - b[i];
        mul[i] = a[i] * b[i];
        div[i] = a[i] / b[i];
        bsum += b[i];
    }
    OIIO_CHECK_SIMD_EQUAL (a+b, add);
    OIIO_CHECK_SIMD_EQUAL (a-b, sub);
    OIIO_CHECK_SIMD_EQUAL (a*b, mul);
    OIIO_CHECK_SIMD_EQUAL_THRESH (a/b, div, eps);
    OIIO_CHECK_SIMD_EQUAL (a*ELEM(2), a*VEC(ELEM(2)));
    OIIO_CHECK_SIMD_EQUAL (ELEM(2)*a, a*VEC(ELEM(2)));
    { VEC r = a; r += b; OIIO_CHECK_SIMD_EQUAL (r, add); }
    { VEC r = a; r -= b; OIIO_CHECK_SIMD_EQUAL (r, sub); }
    { VEC r = a; r *= b; OIIO_CHECK_SIMD_EQUAL (r, mul); }
    { VEC r = a; r /= b; OIIO_CHECK_SIMD_EQUAL_THRESH (r, div, eps); }
    { VEC r = a; r *= ELEM(2); OIIO_CHECK_SIMD_EQUAL (r, a*ELEM(2)); }
    // Test to make sure * works for negative 32 bit ints on all SIMD levels,
    // because it's a different code path for sse2.
    VEC negA = mkvec<VEC>(-1, 1, -2, 2);
    VEC negB = mkvec<VEC>(2, 2, -2, -2);
    OIIO_CHECK_SIMD_EQUAL(negA * negB, mkvec<VEC>(-2, 2, 4, -4));

    OIIO_CHECK_EQUAL (reduce_add(b), bsum);
    OIIO_CHECK_SIMD_EQUAL (vreduce_add(b), VEC(bsum));
    OIIO_CHECK_EQUAL (reduce_add(VEC(1.0f)), SimdElements<VEC>::size);

    benchmark2 ("operator+", do_add<VEC>, a, b);
    benchmark2 ("operator-", do_sub<VEC>, a, b);
    benchmark  ("operator- (neg)", do_neg<VEC>, a);
    benchmark2 ("operator*", do_mul<VEC>, a, b);
    benchmark2 ("operator* (scalar)", do_mul<VEC,ELEM>, a, ELEM(2));
    benchmark2 ("operator/", do_div<VEC>, a, b);
    benchmark  ("abs", do_abs<VEC>, a);
    benchmark  ("reduce_add", [](const VEC& a){ return vreduce_add(a); }, a);
    if (std::is_same<VEC,vfloat3>::value) {  // For vfloat3, compare to Imath
        Imath::V3f a(2.51f,1.0f,1.0f), b(3.1f,1.0f,1.0f);
        benchmark2 ("add Imath::V3f", do_add<Imath::V3f>, a, b, 3 /*work*/);
        benchmark2 ("add Imath::V3f with simd", add_vec_simd, a, b, 3 /*work*/);
        benchmark2 ("sub Imath::V3f", do_sub<Imath::V3f>, a, b, 3 /*work*/);
        benchmark2 ("mul Imath::V3f", do_mul<Imath::V3f>, a, b, 3 /*work*/);
        benchmark2 ("div Imath::V3f", do_div<Imath::V3f>, a, b, 3 /*work*/);
    }
    benchmark2 ("reference: add scalar", do_add<ELEM>, a[2], b[1]);
    benchmark2 ("reference: mul scalar", do_mul<ELEM>, a[2], b[1]);
    benchmark2 ("reference: div scalar", do_div<ELEM>, a[2], b[1]);
}



template<typename VEC>
void test_fused ()
{
    test_heading ("fused ", VEC::type_name());

    VEC a = VEC::Iota (10);
    VEC b = VEC::Iota (1);
    VEC c = VEC::Iota (0.5f);
    OIIO_CHECK_SIMD_EQUAL (madd (a, b, c), a*b+c);
    OIIO_CHECK_SIMD_EQUAL (msub (a, b, c), a*b-c);
    OIIO_CHECK_SIMD_EQUAL (nmadd (a, b, c), -(a*b)+c);
    OIIO_CHECK_SIMD_EQUAL (nmsub (a, b, c), -(a*b)-c);

    benchmark2 ("madd old *+", [&](const VEC& a, const VEC& b){ return a*b+c; }, a, b);
    benchmark2 ("madd fused", [&](const VEC& a, const VEC& b){ return madd(a,b,c); }, a, b);
    benchmark2 ("msub old *-", [&](const VEC& a, const VEC& b){ return a*b-c; }, a, b);
    benchmark2 ("msub fused", [&](const VEC& a, const VEC& b){ return msub(a,b,c); }, a, b);
    benchmark2 ("nmadd old (-*)+", [&](const VEC& a, const VEC& b){ return c-(a*b); }, a, b);
    benchmark2 ("nmadd fused", [&](const VEC& a, const VEC& b){ return nmadd(a,b,c); }, a, b);
    benchmark2 ("nmsub old -(*+)", [&](const VEC& a, const VEC& b){ return -(a*b)-c; }, a, b);
    benchmark2 ("nmsub fused", [&](const VEC& a, const VEC& b){ return nmsub(a,b,c); }, a, b);
}



template<typename T> T do_and (const T& a, const T& b) { return a & b; }
template<typename T> T do_or  (const T& a, const T& b) { return a | b; }
template<typename T> T do_xor (const T& a, const T& b) { return a ^ b; }
template<typename T> T do_compl (const T& a) { return ~a; }
template<typename T> T do_andnot (const T& a, const T& b) { return andnot(a,b); }



template<typename VEC>
void
test_bitwise_int()
{
    test_heading("bitwise ", VEC::type_name());

    VEC a(0x12341234);
    VEC b(0x11111111);
    OIIO_CHECK_SIMD_EQUAL(a & b, VEC(0x10101010));
    OIIO_CHECK_SIMD_EQUAL(a | b, VEC(0x13351335));
    OIIO_CHECK_SIMD_EQUAL(a ^ b, VEC(0x03250325));
    OIIO_CHECK_SIMD_EQUAL(~(a), VEC(0xedcbedcb));
    OIIO_CHECK_SIMD_EQUAL(andnot(b, a), (~(b)) & a);
    OIIO_CHECK_SIMD_EQUAL(andnot(b, a), VEC(0x02240224));

    VEC atest(15);
    atest[1] = 7;
    OIIO_CHECK_EQUAL(reduce_and(atest), 7);

    VEC otest(0);
    otest[1] = 3;
    otest[2] = 4;
    OIIO_CHECK_EQUAL(reduce_or(otest), 7);

    benchmark2("operator&", do_and<VEC>, a, b);
    benchmark2("operator|", do_or<VEC>, a, b);
    benchmark2("operator^", do_xor<VEC>, a, b);
    benchmark("operator!", do_compl<VEC>, a);
    benchmark2("andnot", do_andnot<VEC>, a, b);
    benchmark("reduce_and", [](const VEC& a) { return reduce_and(a); }, a);
    benchmark("reduce_or ", [](const VEC& a) { return reduce_or(a); }, a);
}



template<typename VEC>
void test_bitwise_bool ()
{
    test_heading ("bitwise ", VEC::type_name());

    bool A[]   = { true,  true,  false, false, false, false, true,  true,
                   true,  true,  false, false, false, false, true,  true  };
    bool B[]   = { true,  false, true,  false, true,  false, true,  false,
                   true,  false, true,  false, true,  false, true,  false };
    bool AND[] = { true,  false, false, false, false, false, true,  false,
                   true,  false, false, false, false, false, true,  false };
    bool OR[]  = { true,  true,  true,  false, true,  false, true,  true,
                   true,  true,  true,  false, true,  false, true,  true  };
    bool XOR[] = { false, true,  true,  false, true,  false, false, true,
                   false, true,  true,  false, true,  false, false, true  };
    bool NOT[] = { false, false, true,  true,  true,  true,  false, false,
                   false, false, true,  true,  true,  true,  false, false  };
    VEC a(A), b(B), rand(AND), ror(OR), rxor(XOR), rnot(NOT);
    OIIO_CHECK_SIMD_EQUAL (a & b, rand);
    OIIO_CHECK_SIMD_EQUAL (a | b, ror);
    OIIO_CHECK_SIMD_EQUAL (a ^ b, rxor);
    OIIO_CHECK_SIMD_EQUAL (~a, rnot);

    VEC onebit(false); onebit.setcomp(3,true);
    OIIO_CHECK_EQUAL (reduce_or(VEC::False()), false);
    OIIO_CHECK_EQUAL (reduce_or(onebit), true);
    OIIO_CHECK_EQUAL (reduce_and(VEC::True()), true);
    OIIO_CHECK_EQUAL (reduce_and(onebit), false);
    OIIO_CHECK_EQUAL (all(VEC::True()), true);
    OIIO_CHECK_EQUAL (any(VEC::True()), true);
    OIIO_CHECK_EQUAL (none(VEC::True()), false);
    OIIO_CHECK_EQUAL (all(VEC::False()), false);
    OIIO_CHECK_EQUAL (any(VEC::False()), false);
    OIIO_CHECK_EQUAL (none(VEC::False()), true);

    benchmark2 ("operator&", do_and<VEC>, a, b);
    benchmark2 ("operator|", do_or<VEC>, a, b);
    benchmark2 ("operator^", do_xor<VEC>, a, b);
    benchmark  ("operator!", do_compl<VEC>, a);
    benchmark  ("reduce_and", [](const VEC& a){ return reduce_and(a); }, a);
    benchmark  ("reduce_or ", [](const VEC& a){ return reduce_or(a); }, a);
}



template<class T, class B> B do_lt (const T& a, const T& b) { return a < b; }
template<class T, class B> B do_gt (const T& a, const T& b) { return a > b; }
template<class T, class B> B do_le (const T& a, const T& b) { return a <= b; }
template<class T, class B> B do_ge (const T& a, const T& b) { return a >= b; }
template<class T, class B> B do_eq (const T& a, const T& b) { return a == b; }
template<class T, class B> B do_ne (const T& a, const T& b) { return a != b; }



template<typename VEC>
void
test_comparisons()
{
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::vbool_t bool_t;
    test_heading("comparisons ", VEC::type_name());

    VEC a      = VEC::Iota();
    bool lt2[] = { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    bool gt2[] = { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    bool le2[] = { 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    bool ge2[] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    bool eq2[] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    bool ne2[] = { 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    OIIO_CHECK_SIMD_EQUAL((a < 2), bool_t(lt2));
    OIIO_CHECK_SIMD_EQUAL((a > 2), bool_t(gt2));
    OIIO_CHECK_SIMD_EQUAL((a <= 2), bool_t(le2));
    OIIO_CHECK_SIMD_EQUAL((a >= 2), bool_t(ge2));
    OIIO_CHECK_SIMD_EQUAL((a == 2), bool_t(eq2));
    OIIO_CHECK_SIMD_EQUAL((a != 2), bool_t(ne2));
    VEC b(ELEM(2));
    OIIO_CHECK_SIMD_EQUAL((a < b), bool_t(lt2));
    OIIO_CHECK_SIMD_EQUAL((a > b), bool_t(gt2));
    OIIO_CHECK_SIMD_EQUAL((a <= b), bool_t(le2));
    OIIO_CHECK_SIMD_EQUAL((a >= b), bool_t(ge2));
    OIIO_CHECK_SIMD_EQUAL((a == b), bool_t(eq2));
    OIIO_CHECK_SIMD_EQUAL((a != b), bool_t(ne2));

    benchmark2("operator< ", do_lt<VEC, bool_t>, a, b);
    benchmark2("operator> ", do_gt<VEC, bool_t>, a, b);
    benchmark2("operator<=", do_le<VEC, bool_t>, a, b);
    benchmark2("operator>=", do_ge<VEC, bool_t>, a, b);
    benchmark2("operator==", do_eq<VEC, bool_t>, a, b);
    benchmark2("operator!=", do_ne<VEC, bool_t>, a, b);
}



template<typename VEC>
void
test_shuffle4()
{
    typedef typename VEC::value_t ELEM;
    test_heading("shuffle ", VEC::type_name());

    VEC a(0, 1, 2, 3);
    OIIO_CHECK_SIMD_EQUAL((shuffle<3, 2, 1, 0>(a)), VEC(3, 2, 1, 0));
    OIIO_CHECK_SIMD_EQUAL((shuffle<0, 0, 2, 2>(a)), VEC(0, 0, 2, 2));
    OIIO_CHECK_SIMD_EQUAL((shuffle<1, 1, 3, 3>(a)), VEC(1, 1, 3, 3));
    OIIO_CHECK_SIMD_EQUAL((shuffle<0, 1, 0, 1>(a)), VEC(0, 1, 0, 1));
    OIIO_CHECK_SIMD_EQUAL((shuffle<2>(a)), VEC(ELEM(2)));

    benchmark("shuffle<...> ",
              [&](const VEC& v) { return shuffle<3, 2, 1, 0>(v); }, a);
    benchmark("shuffle<0> ", [&](const VEC& v) { return shuffle<0>(v); }, a);
    benchmark("shuffle<1> ", [&](const VEC& v) { return shuffle<1>(v); }, a);
    benchmark("shuffle<2> ", [&](const VEC& v) { return shuffle<2>(v); }, a);
    benchmark("shuffle<3> ", [&](const VEC& v) { return shuffle<3>(v); }, a);
}



template<typename VEC>
void test_shuffle8 ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("shuffle ", VEC::type_name());
    VEC a (0, 1, 2, 3, 4, 5, 6, 7);
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3,2,1,0,3,2,1,0>(a)), VEC(3,2,1,0,3,2,1,0));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,0,2,2,0,0,2,2>(a)), VEC(0,0,2,2,0,0,2,2));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<1,1,3,3,1,1,3,3>(a)), VEC(1,1,3,3,1,1,3,3));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,1,0,1,0,1,0,1>(a)), VEC(0,1,0,1,0,1,0,1));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<2>(a)), VEC(ELEM(2)));

    benchmark ("shuffle<...> ", [&](const VEC& v){ return shuffle<7,6,5,4,3,2,1,0>(v); }, a);
    benchmark ("shuffle<0> ", [&](const VEC& v){ return shuffle<0>(v); }, a);
    benchmark ("shuffle<1> ", [&](const VEC& v){ return shuffle<1>(v); }, a);
    benchmark ("shuffle<2> ", [&](const VEC& v){ return shuffle<2>(v); }, a);
    benchmark ("shuffle<3> ", [&](const VEC& v){ return shuffle<3>(v); }, a);
    benchmark ("shuffle<4> ", [&](const VEC& v){ return shuffle<4>(v); }, a);
    benchmark ("shuffle<5> ", [&](const VEC& v){ return shuffle<5>(v); }, a);
    benchmark ("shuffle<6> ", [&](const VEC& v){ return shuffle<6>(v); }, a);
    benchmark ("shuffle<7> ", [&](const VEC& v){ return shuffle<7>(v); }, a);
}



template<typename VEC>
void test_shuffle16 ()
{
    test_heading ("shuffle ", VEC::type_name());
    VEC a (0, 1, 2, 3, 4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 15);

    // Shuffle groups of 4
    OIIO_CHECK_SIMD_EQUAL ((shuffle4<3,2,1,0>(a)),
                           VEC(12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3));
    OIIO_CHECK_SIMD_EQUAL ((shuffle4<3>(a)),
                           VEC(12,13,14,15,12,13,14,15,12,13,14,15,12,13,14,15));

    // Shuffle within groups of 4
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3,2,1,0>(a)),
                           VEC(3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3>(a)),
                           VEC(3,3,3,3,7,7,7,7,11,11,11,11,15,15,15,15));

    benchmark ("shuffle4<> ", [&](const VEC& v){ return shuffle<3,2,1,0>(v); }, a);
    benchmark ("shuffle<> ",  [&](const VEC& v){ return shuffle<3,2,1,0>(v); }, a);
}



template<typename VEC>
void
test_swizzle()
{
    test_heading("swizzle ", VEC::type_name());

    VEC a = VEC::Iota(0);
    VEC b = VEC::Iota(10);
    OIIO_CHECK_SIMD_EQUAL(AxyBxy(a, b), VEC(0, 1, 10, 11));
    OIIO_CHECK_SIMD_EQUAL(AxBxAyBy(a, b), VEC(0, 10, 1, 11));
    OIIO_CHECK_SIMD_EQUAL(b.xyz0(), VEC(10, 11, 12, 0));
    OIIO_CHECK_SIMD_EQUAL(b.xyz1(), VEC(10, 11, 12, 1));
}



template<typename VEC>
void test_blend ()
{
    test_heading ("blend ", VEC::type_name());
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::vbool_t bool_t;

    VEC a = VEC::Iota (1);
    VEC b = VEC::Iota (10);
    bool_t f(false), t(true);
    bool tf_values[] = { true, false, true, false, true, false, true, false,
                         true, false, true, false, true, false, true, false };
    bool_t tf ((bool *)tf_values);

    OIIO_CHECK_SIMD_EQUAL (blend (a, b, f), a);
    OIIO_CHECK_SIMD_EQUAL (blend (a, b, t), b);

    ELEM r1[] = { 10, 2, 12, 4, 14, 6, 16, 8,  18, 10, 20, 12, 22, 14, 24, 16 };
    OIIO_CHECK_SIMD_EQUAL (blend (a, b, tf), VEC(r1));

    OIIO_CHECK_SIMD_EQUAL (blend0 (a, f), VEC::Zero());
    OIIO_CHECK_SIMD_EQUAL (blend0 (a, t), a);
    ELEM r2[] = { 1, 0, 3, 0, 5, 0, 7, 0,  9, 0, 11, 0, 13, 0, 15, 0 };
    OIIO_CHECK_SIMD_EQUAL (blend0 (a, tf), VEC(r2));

    OIIO_CHECK_SIMD_EQUAL (blend0not (a, f), a);
    OIIO_CHECK_SIMD_EQUAL (blend0not (a, t), VEC::Zero());
    ELEM r3[] = { 0, 2, 0, 4, 0, 6, 0, 8,  0, 10, 0, 12, 0, 14, 0, 16 };
    OIIO_CHECK_SIMD_EQUAL (blend0not (a, tf), VEC(r3));

    benchmark2 ("blend", [&](const VEC& a, const VEC& b){ return blend(a,b,tf); }, a, b);
    benchmark2 ("blend0", [](const VEC& a, const bool_t& b){ return blend0(a,b); }, a, tf);
    benchmark2 ("blend0not", [](const VEC& a, const bool_t& b){ return blend0not(a,b); }, a, tf);
}



template<typename VEC>
void
test_transpose4()
{
    test_heading("transpose ", VEC::type_name());

    VEC a(0, 1, 2, 3);
    VEC b(4, 5, 6, 7);
    VEC c(8, 9, 10, 11);
    VEC d(12, 13, 14, 15);

    OIIO_CHECK_SIMD_EQUAL(AxBxCxDx(a, b, c, d), VEC(0, 4, 8, 12));

    std::cout << " before transpose:\n";
    std::cout << "\t" << a << "\n";
    std::cout << "\t" << b << "\n";
    std::cout << "\t" << c << "\n";
    std::cout << "\t" << d << "\n";
    transpose(a, b, c, d);
    std::cout << " after transpose:\n";
    std::cout << "\t" << a << "\n";
    std::cout << "\t" << b << "\n";
    std::cout << "\t" << c << "\n";
    std::cout << "\t" << d << "\n";
    OIIO_CHECK_SIMD_EQUAL(a, VEC(0, 4, 8, 12));
    OIIO_CHECK_SIMD_EQUAL(b, VEC(1, 5, 9, 13));
    OIIO_CHECK_SIMD_EQUAL(c, VEC(2, 6, 10, 14));
    OIIO_CHECK_SIMD_EQUAL(d, VEC(3, 7, 11, 15));
}



template<typename T> inline T do_shl (const T &a, int b) { return a<<b; }
template<typename T> inline T do_shr (const T &a, int b) { return a>>b; }
template<typename T> inline T do_srl (const T &a, int b) { return srl(a,b); }
template<typename T> inline T do_rotl (const T &a, int b) { return rotl(a,b); }


template<typename VEC>
void
test_shift()
{
    test_heading("shift ", VEC::type_name());

    // Basics of << and >>
    VEC i = VEC::Iota(10, 10);  // 10, 20, 30 ...
    OIIO_CHECK_SIMD_EQUAL(i << 2, VEC::Iota(40, 40));
    OIIO_CHECK_SIMD_EQUAL(i >> 1, VEC::Iota(5, 5));

    // Tricky cases with high bits, and the difference between >> and srl
    int vals[4] = { 1 << 31, -1, 0xffff, 3 };
    for (auto hard : vals) {
        VEC vhard(hard);
        OIIO_CHECK_SIMD_EQUAL (vhard >> 1, VEC(hard>>1));
        OIIO_CHECK_SIMD_EQUAL (srl(vhard,1), VEC(unsigned(hard)>>1));
        Strutil::print("  [{:x}] >>  1 == [{:x}]\n", vhard, vhard>>1);
        Strutil::print("  [{:x}] srl 1 == [{:x}]\n", vhard, srl(vhard,1));
        OIIO_CHECK_SIMD_EQUAL (srl(vhard,4), VEC(unsigned(hard)>>4));
        Strutil::print("  [{:x}] >>  4 == [{:x}]\n", vhard, vhard>>4);
        Strutil::print("  [{:x}] srl 4 == [{:x}]\n", vhard, srl(vhard,4));
    }

    // Test <<= and >>=
    i = VEC::Iota (10, 10);   i <<= 2;
    OIIO_CHECK_SIMD_EQUAL (i, VEC::Iota(40, 40));
    i = VEC::Iota (10, 10);   i >>= 1;
    OIIO_CHECK_SIMD_EQUAL (i, VEC::Iota(5, 5));

    // Test rotl
    {
        vint4 v (0x12345678, 0xabcdef01, 0x98765432, 0x31415926);
        vint4 r (0x23456781, 0xbcdef01a, 0x87654329, 0x14159263);
        OIIO_CHECK_SIMD_EQUAL (rotl(v,4), r);
    }

    // Benchmark
    benchmark2 ("operator<<", do_shl<VEC>, i, 2);
    benchmark2 ("operator>>", do_shr<VEC>, i, 2);
    benchmark2 ("srl       ", do_srl<VEC>, i, 2);
    benchmark2 ("rotl      ", do_rotl<VEC>, i, 2);
}



void
test_vectorops_vfloat4()
{
    typedef vfloat4 VEC;
    typedef VEC::value_t ELEM;
    test_heading("vectorops ", VEC::type_name());

    VEC a = mkvec<VEC> (10, 11, 12, 13);
    VEC b = mkvec<VEC> (1, 2, 3, 4);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36+52));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36+52));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (hdiv(vfloat4(1.0f,2.0f,3.0f,2.0f)), vfloat3(0.5f,1.0f,1.5f));

    benchmark2 ("vdot", [](const VEC& a, const VEC& b){ return vdot(a,b); }, a, b);
    benchmark2 ("dot", [](const VEC& a, const VEC& b){ return dot(a,b); }, a, b);
    benchmark2 ("vdot3", [](const VEC& a, const VEC& b){ return vdot3(a,b); }, a, b);
    benchmark2 ("dot3", [](const VEC& a, const VEC& b){ return dot3(a,b); }, a, b);
}



void test_vectorops_vfloat3 ()
{
    typedef vfloat3 VEC;
    typedef VEC::value_t ELEM;
    test_heading ("vectorops ", VEC::type_name());

    VEC a = mkvec<VEC> (10, 11, 12);
    VEC b = mkvec<VEC> (1, 2, 3);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vfloat3(1.0f,2.0f,3.0f).normalized(),
                           vfloat3(norm_imath(Imath::V3f(1.0f,2.0f,3.0f))));
    OIIO_CHECK_SIMD_EQUAL_THRESH (vfloat3(1.0f,2.0f,3.0f).normalized_fast(),
                                  vfloat3(norm_imath(Imath::V3f(1.0f,2.0f,3.0f))), 0.0005);

    benchmark2 ("vdot", [](const VEC& a, const VEC& b){ return vdot(a,b); }, a, b);
    benchmark2 ("dot", [](const VEC& a, const VEC& b){ return dot(a,b); }, a, b);
    benchmark ("dot vfloat3", dot_simd, vfloat3(2.0f,1.0f,0.0f), 1);
    // benchmark2 ("dot Imath::V3f", [](Imath::V3f& a, Imath::V3f& b){ return a.dot(b); }, a.V3f(), b.V3f());
    benchmark ("dot Imath::V3f", dot_imath, Imath::V3f(2.0f,1.0f,0.0f), 1);
    benchmark ("dot Imath::V3f with simd", dot_imath_simd, Imath::V3f(2.0f,1.0f,0.0f), 1);
    benchmark ("normalize Imath", norm_imath, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark ("normalize Imath with simd", norm_imath_simd, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark ("normalize Imath with simd fast", norm_imath_simd_fast, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark ("normalize simd", norm_simd, vfloat3(1.0f,4.0f,9.0f));
    benchmark ("normalize simd fast", norm_simd_fast, vfloat3(1.0f,4.0f,9.0f));
}



void test_constants ()
{
    test_heading ("constants");

    OIIO_CHECK_SIMD_EQUAL (vbool4::False(), vbool4(false));
    OIIO_CHECK_SIMD_EQUAL (vbool4::True(), vbool4(true));

    OIIO_CHECK_SIMD_EQUAL (vbool8::False(), vbool8(false));
    OIIO_CHECK_SIMD_EQUAL (vbool8::True(), vbool8(true));

    OIIO_CHECK_SIMD_EQUAL (vbool16::False(), vbool16(false));
    OIIO_CHECK_SIMD_EQUAL (vbool16::True(), vbool16(true));
    OIIO_CHECK_SIMD_EQUAL (vbool16::False(), vbool16(false));
    OIIO_CHECK_SIMD_EQUAL (vbool16::True(), vbool16(true));

    OIIO_CHECK_SIMD_EQUAL (vint4::Zero(), vint4(0));
    OIIO_CHECK_SIMD_EQUAL (vint4::One(), vint4(1));
    OIIO_CHECK_SIMD_EQUAL (vint4::NegOne(), vint4(-1));
    OIIO_CHECK_SIMD_EQUAL (vint4::Iota(), vint4(0,1,2,3));
    OIIO_CHECK_SIMD_EQUAL (vint4::Iota(3), vint4(3,4,5,6));
    OIIO_CHECK_SIMD_EQUAL (vint4::Iota(3,2), vint4(3,5,7,9));
    OIIO_CHECK_SIMD_EQUAL (vint4::Giota(), vint4(1,2,4,8));

    OIIO_CHECK_SIMD_EQUAL (vint8::Zero(), vint8(0));
    OIIO_CHECK_SIMD_EQUAL (vint8::One(), vint8(1));
    OIIO_CHECK_SIMD_EQUAL (vint8::NegOne(), vint8(-1));
    OIIO_CHECK_SIMD_EQUAL (vint8::Iota(), vint8(0,1,2,3, 4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (vint8::Iota(3), vint8(3,4,5,6, 7,8,9,10));
    OIIO_CHECK_SIMD_EQUAL (vint8::Iota(3,2), vint8(3,5,7,9, 11,13,15,17));
    OIIO_CHECK_SIMD_EQUAL (vint8::Giota(), vint8(1,2,4,8, 16,32,64,128));

    OIIO_CHECK_SIMD_EQUAL (vint16::Zero(), vint16(0));
    OIIO_CHECK_SIMD_EQUAL (vint16::One(), vint16(1));
    OIIO_CHECK_SIMD_EQUAL (vint16::NegOne(), vint16(-1));
    OIIO_CHECK_SIMD_EQUAL (vint16::Iota(), vint16(0,1,2,3, 4,5,6,7, 8,9,10,11, 12,13,14,15));
    OIIO_CHECK_SIMD_EQUAL (vint16::Iota(3), vint16(3,4,5,6, 7,8,9,10, 11,12,13,14, 15,16,17,18));
    OIIO_CHECK_SIMD_EQUAL (vint16::Iota(3,2), vint16(3,5,7,9, 11,13,15,17, 19,21,23,25, 27,29,31,33));
    OIIO_CHECK_SIMD_EQUAL (vint16::Giota(), vint16(1,2,4,8, 16,32,64,128, 256,512,1024,2048, 4096,8192,16384,32768));

    OIIO_CHECK_SIMD_EQUAL (vfloat4::Zero(), vfloat4(0.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat4::One(), vfloat4(1.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat4::Iota(), vfloat4(0,1,2,3));
    OIIO_CHECK_SIMD_EQUAL (vfloat4::Iota(3.0f), vfloat4(3,4,5,6));
    OIIO_CHECK_SIMD_EQUAL (vfloat4::Iota(3.0f,2.0f), vfloat4(3,5,7,9));

    OIIO_CHECK_SIMD_EQUAL (vfloat3::Zero(), vfloat3(0.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat3::One(), vfloat3(1.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat3::Iota(), vfloat3(0,1,2));
    OIIO_CHECK_SIMD_EQUAL (vfloat3::Iota(3.0f), vfloat3(3,4,5));
    OIIO_CHECK_SIMD_EQUAL (vfloat3::Iota(3.0f,2.0f), vfloat3(3,5,7));

    OIIO_CHECK_SIMD_EQUAL (vfloat8::Zero(), vfloat8(0.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat8::One(), vfloat8(1.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat8::Iota(), vfloat8(0,1,2,3,4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (vfloat8::Iota(3.0f), vfloat8(3,4,5,6,7,8,9,10));
    OIIO_CHECK_SIMD_EQUAL (vfloat8::Iota(3.0f,2.0f), vfloat8(3,5,7,9,11,13,15,17));

    OIIO_CHECK_SIMD_EQUAL (vfloat16::Zero(), vfloat16(0.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat16::One(), vfloat16(1.0f));
    OIIO_CHECK_SIMD_EQUAL (vfloat16::Iota(), vfloat16(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15));
    OIIO_CHECK_SIMD_EQUAL (vfloat16::Iota(3.0f), vfloat16(3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18));
    OIIO_CHECK_SIMD_EQUAL (vfloat16::Iota(3.0f,2.0f), vfloat16(3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33));

    benchmark ("vfloat4 = float(const)", [](float f){ return vfloat4(f); }, 1.0f);
    benchmark ("vfloat4 = Zero()", [](int){ return vfloat4::Zero(); }, 0);
    benchmark ("vfloat4 = One()", [](int){ return vfloat4::One(); }, 0);
    benchmark ("vfloat4 = Iota()", [](int){ return vfloat4::Iota(); }, 0);

    benchmark ("vfloat8 = float(const)", [](float f){ return vfloat8(f); }, 1.0f);
    benchmark ("vfloat8 = Zero()", [](int){ return vfloat8::Zero(); }, 0);
    benchmark ("vfloat8 = One()", [](int){ return vfloat8::One(); }, 0);
    benchmark ("vfloat8 = Iota()", [](int){ return vfloat8::Iota(); }, 0);

    benchmark ("vfloat16 = float(const)", [](float f){ return vfloat16(f); }, 1.0f);
    benchmark ("vfloat16 = Zero()", [](int){ return vfloat16::Zero(); }, 0);
    benchmark ("vfloat16 = One()", [](int){ return vfloat16::One(); }, 0);
    benchmark ("vfloat16 = Iota()", [](int){ return vfloat16::Iota(); }, 0);
}



// Miscellaneous one-off stuff not caught by other tests
void
test_special()
{
    test_heading("special");
    {
        // Make sure a vfloat4 constructed from saturated unsigned short,
        // short, unsigned char, or char values, then divided by the float
        // max, exactly equals 1.0.
        short s32767[] = {32767, 32767, 32767, 32767};
        unsigned short us65535[] = {65535, 65535, 65535, 65535};
        char c127[] = {127, 127, 127, 127};
        unsigned char uc255[] = {255, 255, 255, 255};
        OIIO_CHECK_SIMD_EQUAL (vfloat4(us65535)/vfloat4(65535.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(us65535)*vfloat4(1.0f/65535.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(s32767)/vfloat4(32767.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(s32767)*vfloat4(1.0f/32767.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(uc255)/vfloat4(255.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(uc255)*vfloat4(1.0f/255.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(c127)/vfloat4(127.0), vfloat4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (vfloat4(c127)*vfloat4(1.0f/127.0), vfloat4(1.0f));
    }

    // Test the 2-vfloat4 shuffle
    {
        #define PERMUTE(a,b,c,d) ((d<<6)|(c<<4)|(b<<2)|(a<<0))
        vfloat4 a(10, 11, 12, 13);
        vfloat4 b(20, 21, 22, 23);
        OIIO_CHECK_SIMD_EQUAL(shuffle<PERMUTE(2,0,1,3)>(a,b),
                              vfloat4(12, 10, 21, 23));
    }
    // Test vfloat4::load_pairs
    {
        vfloat4 x;
        static const float vals[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
        x.load_pairs(vals+2, vals+5);
        OIIO_CHECK_SIMD_EQUAL(x, vfloat4(2, 3, 5, 6));
    }
}



// Wrappers to resolve the return type ambiguity
inline float fast_exp_float (float x) { return fast_exp(x); }
inline vfloat4 fast_exp_vfloat4 (const vfloat4& x) { return fast_exp(x); }
inline float fast_log_float (float x) { return fast_log(x); }
//inline vfloat4 fast_log_float (const vfloat4& x) { return fast_log(x); }
inline float rsqrtf (float f) { return 1.0f / sqrtf(f); }
inline float rcp (float f) { return 1.0f / f; }



template<typename VEC>
void test_mathfuncs ()
{
    typedef typename VEC::vint_t vint_t;
    test_heading ("mathfuncs", VEC::type_name());

    VEC F = mkvec<VEC> (-1.5f, 0.0f, 1.9f, 4.1f);
    OIIO_CHECK_SIMD_EQUAL (abs(F), mkvec<VEC>(std::abs(F[0]), std::abs(F[1]), std::abs(F[2]), std::abs(F[3])));
    // OIIO_CHECK_SIMD_EQUAL (sign(F), mkvec<VEC>(std::sign(F[0]), std::sign(F[1]), std::sign(F[2]), std::sign(F[3])));
    OIIO_CHECK_SIMD_EQUAL (ceil(F), mkvec<VEC>(std::ceil(F[0]), std::ceil(F[1]), std::ceil(F[2]), std::ceil(F[3])));
    OIIO_CHECK_SIMD_EQUAL (floor(F), mkvec<VEC>(std::floor(F[0]), std::floor(F[1]), std::floor(F[2]), std::floor(F[3])));
    OIIO_CHECK_SIMD_EQUAL (round(F), mkvec<VEC>(std::round(F[0]), std::round(F[1]), std::round(F[2]), std::round(F[3])));
    benchmark ("simd abs", [](const VEC& v){ return abs(v); }, 1.1f);
    benchmark ("simd sign", [](const VEC& v){ return sign(v); }, 1.1f);
    benchmark ("simd ceil", [](const VEC& v){ return ceil(v); }, 1.1f);
    benchmark ("simd floor", [](const VEC& v){ return floor(v); }, 1.1f);
    benchmark ("simd round", [](const VEC& v){ return round(v); }, 1.1f);

    VEC A = mkvec<VEC> (-1.0f, 0.0f, 1.0f, 4.5f);
    VEC expA = mkvec<VEC> (0.367879441171442f, 1.0f, 2.718281828459045f, 90.0171313005218f);
    OIIO_CHECK_SIMD_EQUAL (exp(A), expA);
    OIIO_CHECK_SIMD_EQUAL_THRESH (log(expA), A, 1e-6f);
    OIIO_CHECK_SIMD_EQUAL_THRESH (fast_exp(A),
                mkvec<VEC>(fast_exp(A[0]), fast_exp(A[1]), fast_exp(A[2]), fast_exp(A[3])), 1e-5f);
    OIIO_CHECK_SIMD_EQUAL_THRESH (fast_log(expA),
                mkvec<VEC>(fast_log(expA[0]), fast_log(expA[1]), fast_log(expA[2]), fast_log(expA[3])), 0.00001f);
    OIIO_CHECK_SIMD_EQUAL_THRESH (fast_pow_pos(VEC(2.0f), A),
                           mkvec<VEC>(0.5f, 1.0f, 2.0f, 22.62741699796952f), 0.0001f);

    OIIO_CHECK_SIMD_EQUAL (safe_div(mkvec<VEC>(1.0f,2.0f,3.0f,4.0f), mkvec<VEC>(2.0f,0.0f,2.0f,0.0f)),
                           mkvec<VEC>(0.5f,0.0f,1.5f,0.0f));
    OIIO_CHECK_SIMD_EQUAL_THRESH (sqrt(mkvec<VEC>(1.0f,4.0f,9.0f,16.0f)), mkvec<VEC>(1.0f,2.0f,3.0f,4.0f), 0.00001);
    OIIO_CHECK_SIMD_EQUAL_THRESH (rsqrt(mkvec<VEC>(1.0f,4.0f,9.0f,16.0f)), VEC(1.0f)/mkvec<VEC>(1.0f,2.0f,3.0f,4.0f), 0.00001);
    OIIO_CHECK_SIMD_EQUAL_THRESH (rsqrt_fast(mkvec<VEC>(1.0f,4.0f,9.0f,16.0f)),
                                  VEC(1.0f)/mkvec<VEC>(1.0f,2.0f,3.0f,4.0f), 0.0005f);
    OIIO_CHECK_SIMD_EQUAL_THRESH (rcp_fast(VEC::Iota(1.0f)),
                                  VEC(1.0f)/VEC::Iota(1.0f), 0.0005f);

    benchmark2 ("simd operator/", do_div<VEC>, A, A);
    benchmark2 ("simd safe_div", do_safe_div<VEC>, A, A);
    benchmark ("simd rcp_fast", [](const VEC& v){ return rcp_fast(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));

    OIIO_CHECK_SIMD_EQUAL (ifloor(mkvec<VEC>(0.0f, 0.999f, 1.0f, 1.001f)),
                           mkvec<vint_t>(0, 0, 1, 1));
    OIIO_CHECK_SIMD_EQUAL (ifloor(mkvec<VEC>(0.0f, -0.999f, -1.0f, -1.001f)),
                           mkvec<vint_t>(0, -1, -1, -2));
    benchmark ("float ifloor", [](float&v){ return ifloor(v); }, 1.1f);
    benchmark ("simd ifloor", [](const VEC&v){ return simd::ifloor(v); }, VEC(1.1f));

    int iscalar;
    vint_t ival;
    VEC fval = -1.1;
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(0.0f),    &ival), 0.0f);   OIIO_CHECK_SIMD_EQUAL (ival, 0);
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(-0.999f), &ival), 0.001f); OIIO_CHECK_SIMD_EQUAL (ival, -1);
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(-1.0f),   &ival), 0.0f);   OIIO_CHECK_SIMD_EQUAL (ival, -1);
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(-1.001f), &ival), 0.999f); OIIO_CHECK_SIMD_EQUAL (ival, -2);
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(0.999f),  &ival), 0.999f); OIIO_CHECK_SIMD_EQUAL (ival, 0);
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(1.0f),    &ival), 0.0f);   OIIO_CHECK_SIMD_EQUAL (ival, 1);
    OIIO_CHECK_EQUAL_APPROX (floorfrac(VEC(1.001f),  &ival), 0.001f); OIIO_CHECK_SIMD_EQUAL (ival, 1);
    benchmark ("float floorfrac", [&](float x){ return DoNotOptimize(floorfrac(x,&iscalar)); }, 1.1f);
    benchmark ("simd floorfrac", [&](const VEC& x){ return DoNotOptimize(floorfrac(x,&ival)); }, fval);

    benchmark ("float expf", expf, 0.67f);
    benchmark ("float fast_exp", fast_exp_float, 0.67f);
    benchmark ("simd exp", [](const VEC& v){ return simd::exp(v); }, VEC(0.67f));
    benchmark ("simd fast_exp", [](const VEC& v){ return fast_exp(v); }, VEC(0.67f));

    benchmark ("float logf", logf, 0.67f);
    benchmark ("fast_log", fast_log_float, 0.67f);
    benchmark ("simd log", [](const VEC& v){ return simd::log(v); }, VEC(0.67f));
    benchmark ("simd fast_log", fast_log<VEC>, VEC(0.67f));
    benchmark2 ("float powf", powf, 0.67f, 0.67f);
    benchmark2 ("simd fast_pow_pos", [](const VEC& x,const VEC& y){ return fast_pow_pos(x,y); }, VEC(0.67f), VEC(0.67f));
    benchmark ("float sqrt", sqrtf, 4.0f);
    benchmark ("simd sqrt", [](const VEC& v){ return sqrt(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
    benchmark ("float rsqrt", rsqrtf, 4.0f);
    benchmark ("simd rsqrt", [](const VEC& v){ return rsqrt(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
    benchmark ("simd rsqrt_fast", [](const VEC& v){ return rsqrt_fast(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
}



void test_metaprogramming ()
{
    test_heading ("metaprogramming");
    OIIO_CHECK_EQUAL (SimdSize<vfloat4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<vfloat3>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<vint4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<vbool4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<vfloat8>::size, 8);
    OIIO_CHECK_EQUAL (SimdSize<vint8>::size, 8);
    OIIO_CHECK_EQUAL (SimdSize<vbool8>::size, 8);
    OIIO_CHECK_EQUAL (SimdSize<vfloat16>::size, 16);
    OIIO_CHECK_EQUAL (SimdSize<vint16>::size, 16);
    OIIO_CHECK_EQUAL (SimdSize<vbool16>::size, 16);
    OIIO_CHECK_EQUAL (SimdSize<float>::size, 1);
    OIIO_CHECK_EQUAL (SimdSize<int>::size, 1);
    OIIO_CHECK_EQUAL (SimdSize<bool>::size, 1);

    OIIO_CHECK_EQUAL (SimdElements<vfloat4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<vfloat3>::size, 3);
    OIIO_CHECK_EQUAL (SimdElements<vint4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<vbool4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<vfloat8>::size, 8);
    OIIO_CHECK_EQUAL (SimdElements<vint8>::size, 8);
    OIIO_CHECK_EQUAL (SimdElements<vbool8>::size, 8);
    OIIO_CHECK_EQUAL (SimdElements<vfloat16>::size, 16);
    OIIO_CHECK_EQUAL (SimdElements<vint16>::size, 16);
    OIIO_CHECK_EQUAL (SimdElements<vbool16>::size, 16);
    OIIO_CHECK_EQUAL (SimdElements<float>::size, 1);
    OIIO_CHECK_EQUAL (SimdElements<int>::size, 1);
    OIIO_CHECK_EQUAL (SimdElements<bool>::size, 1);

    OIIO_CHECK_EQUAL (vfloat4::elements, 4);
    OIIO_CHECK_EQUAL (vfloat3::elements, 3);
    OIIO_CHECK_EQUAL (vint4::elements, 4);
    OIIO_CHECK_EQUAL (vbool4::elements, 4);
    // OIIO_CHECK_EQUAL (vfloat8::elements, 8);
    OIIO_CHECK_EQUAL (vint8::elements, 8);
    OIIO_CHECK_EQUAL (vbool8::elements, 8);
    OIIO_CHECK_EQUAL (vfloat16::elements, 16);
    OIIO_CHECK_EQUAL (vint16::elements, 16);
    OIIO_CHECK_EQUAL (vbool16::elements, 16);

    // Make sure that VTYPE::value_t returns the right element type
    OIIO_CHECK_ASSERT((std::is_same<vfloat4::value_t, float>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat3::value_t, float>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat8::value_t, float>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat16::value_t, float>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint4::value_t, int>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint8::value_t, int>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint16::value_t, int>::value));
    OIIO_CHECK_ASSERT((std::is_same<vbool4::value_t, bool>::value));
    OIIO_CHECK_ASSERT((std::is_same<vbool8::value_t, bool>::value));
    OIIO_CHECK_ASSERT((std::is_same<vbool16::value_t, bool>::value));

    // Make sure that VTYPE::vfloat_t returns the same-sized float type
    OIIO_CHECK_ASSERT((std::is_same<vfloat4::vfloat_t, vfloat4>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat8::vfloat_t, vfloat8>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat16::vfloat_t, vfloat16>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint4::vfloat_t, vfloat4>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint8::vfloat_t, vfloat8>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint16::vfloat_t, vfloat16>::value));

    // Make sure that VTYPE::vint_t returns the same-sized int type
    OIIO_CHECK_ASSERT((std::is_same<vfloat4::vint_t, vint4>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat8::vint_t, vint8>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat16::vint_t, vint16>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint4::vint_t, vint4>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint8::vint_t, vint8>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint16::vint_t, vint16>::value));

    // Make sure that VTYPE::vbool_t returns the same-sized bool type
    OIIO_CHECK_ASSERT((std::is_same<vfloat4::vbool_t, vbool4>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat8::vbool_t, vbool8>::value));
    OIIO_CHECK_ASSERT((std::is_same<vfloat16::vbool_t, vbool16>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint4::vbool_t, vbool4>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint8::vbool_t, vbool8>::value));
    OIIO_CHECK_ASSERT((std::is_same<vint16::vbool_t, vbool16>::value));
}



// Transform a point by a matrix using regular Imath
inline Imath::V3f
transformp_imath(const Imath::V3f& v, const Imath::M44f& m)
{
    Imath::V3f r;
    m.multVecMatrix(v, r);
    return r;
}

// Transform a point by a matrix using simd ops on Imath types.
inline Imath::V3f
transformp_imath_simd(const Imath::V3f& v, const Imath::M44f& m)
{
    return simd::transformp(m, v).V3f();
}

// Transform a simd point by an Imath matrix using SIMD
inline vfloat3
transformp_simd(const vfloat3& v, const Imath::M44f& m)
{
    return simd::transformp(m, v);
}

// Transform a point by a matrix using regular Imath
inline Imath::V3f
transformv_imath(const Imath::V3f& v, const Imath::M44f& m)
{
    Imath::V3f r;
    m.multDirMatrix(v, r);
    return r;
}

inline Imath::V4f
mul_vm_imath(const Imath::V4f& v, const Imath::M44f& m)
{
    return v*m;
}

// inline Imath::V4f
// mul_mv_imath(const Imath::M44f& m, const Imath::V4f& v)
// {
//     return m*v;
// }

inline vfloat4
mul_vm_simd(const vfloat4& v, const matrix44& m)
{
    return v*m;
}

inline vfloat4
mul_mv_simd(const matrix44& m, const vfloat4 v)
{
    return m*v;
}



inline bool
mx_equal_thresh(const matrix44& a, const matrix44& b, float thresh)
{
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i)
            if (fabsf(a[j][i] - b[j][i]) > thresh)
                return false;
    return true;
}



inline Imath::M44f
mat_transpose(const Imath::M44f& m)
{
    return m.transposed();
}

inline Imath::M44f
mat_transpose_simd(const Imath::M44f& m)
{
    return matrix44(m).transposed().M44f();
}



void
test_matrix()
{
    Imath::V3f P(1.0f, 0.0f, 0.0f);
    Imath::M44f Mtrans(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 11, 12, 1);
    Imath::M44f Mrot = Imath::M44f().rotate(Imath::V3f(0.0f, M_PI/4.0f, 0.0f));

    test_heading("Testing matrix ops:");
    std::cout << "  P = " << P << "\n";
    std::cout << "  Mtrans = " << Mtrans << "\n";
    std::cout << "  Mrot   = " << Mrot << "\n";
    OIIO_CHECK_EQUAL(simd::transformp(Mtrans, P).V3f(),
                     transformp_imath(P, Mtrans));
    std::cout << "  P translated = " << simd::transformp(Mtrans, P) << "\n";
    OIIO_CHECK_EQUAL(simd::transformv(Mtrans, P).V3f(), P);
    OIIO_CHECK_EQUAL(simd::transformp(Mrot, P).V3f(),
                     transformp_imath(P, Mrot));
    std::cout << "  P rotated = " << simd::transformp(Mrot, P) << "\n";
    OIIO_CHECK_EQUAL(simd::transformvT(Mrot, P).V3f(),
                     transformv_imath(P, Mrot.transposed()));
    std::cout << "  P rotated by the transpose = " << simd::transformv(Mrot, P)
              << "\n";
    OIIO_CHECK_EQUAL(matrix44(Mrot).transposed().M44f(), Mrot.transposed());
    std::cout << "  Mrot transposed = " << matrix44(Mrot).transposed().M44f()
              << "\n";

    // Test m44 * v4, v4 * m44
    {
        Imath::M44f M(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
        matrix44 m(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
        Imath::V4f V(1,2,3,4);
        vfloat4 v(1,2,3,4);
        vfloat4 vm = v*m;
        OIIO_CHECK_SIMD_EQUAL(vm, vfloat4(V*M));
        // vfloat4 mv = m*v;
        // OIIO_CHECK_SIMD_EQUAL(mv, M*V);
        benchmark2("V4 * M44 Imath", mul_vm_imath, V, M, 1);
        // benchmark2("M44 * V4 Imath", mul_mv_imath, mx, v4x, 1);
        benchmark2("M44 * V4 simd", mul_mv_simd, m, v, 1);
        benchmark2("V4 * M44 simd", mul_vm_simd, v, m, 1);
    }

    // Test ==, !=
    {
        matrix44 mt(Mtrans), mr(Mrot);
        OIIO_CHECK_EQUAL(mt, mt);
        OIIO_CHECK_EQUAL(mt, Mtrans);
        OIIO_CHECK_EQUAL(Mtrans, mt);
        OIIO_CHECK_NE(mt, mr);
        OIIO_CHECK_NE(mr, Mtrans);
        OIIO_CHECK_NE(Mtrans, mr);
    }
    OIIO_CHECK_ASSERT(
        mx_equal_thresh(matrix44(Mtrans.inverse()), matrix44(Mtrans).inverse(),
                        1.0e-6f));
    OIIO_CHECK_ASSERT(
        mx_equal_thresh(matrix44(Mrot.inverse()), matrix44(Mrot).inverse(),
                        1.0e-6f));

    // Test that matrix44::inverse always matches Imath::M44f::inverse
    Imath::M44f rts = (Mtrans * Mrot) * Imath::M44f(2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    OIIO_CHECK_ASSERT(
        mx_equal_thresh(matrix44(rts.inverse()), matrix44(rts).inverse(),
                        1.0e-5f));
    OIIO_CHECK_ASSERT(
        mx_equal_thresh(matrix44(Mtrans.inverse()), matrix44(Mtrans).inverse(),
                        1.0e-6f));
    OIIO_CHECK_ASSERT(
        mx_equal_thresh(matrix44(Mrot.inverse()), matrix44(Mrot).inverse(),
                        1.0e-6f));
    Imath::M44f m123(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 1.0f);
    OIIO_CHECK_ASSERT(
        mx_equal_thresh(matrix44(m123.inverse()), matrix44(m123).inverse(),
                        1.0e-6f));    

    OIIO_CHECK_EQUAL(
        matrix44(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
        Imath::M44f(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));

    Imath::V3f vx(2.51f, 1.0f, 1.0f);
    Imath::M44f mx(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 11, 12, 1);
    benchmark2("transformp Imath", transformp_imath, vx, mx, 1);
    benchmark2("transformp Imath with simd", transformp_imath_simd, vx, mx, 1);
    benchmark2("transformp simd", transformp_simd, vfloat3(vx), mx, 1);

    benchmark("transpose m44", mat_transpose, mx, 1);
    benchmark("transpose m44 with simd", mat_transpose_simd, mx, 1);
    // Reduce the iterations of the ones below, if we can
    iterations /= 2;
    benchmark("m44 inverse Imath", inverse_imath, mx, 1);
    // std::cout << "inv " << matrix44(inverse_imath(mx)) << "\n";
    benchmark("m44 inverse_simd", inverse_simd, matrix44(mx), 1);
    // std::cout << "inv " << inverse_simd(mx) << "\n";
    benchmark("m44 inverse_simd native simd", inverse_simd, matrix44(mx), 1);
    // std::cout << "inv " << inverse_simd(mx) << "\n";
    iterations *= 2;  // put things the way they were
}



static void
test_trivially_copyable()
{
    print("\nTesting trivially_copyable on all SIMD classes\n");
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vbool4>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vint4>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vfloat4>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vfloat3>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<matrix44>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vbool8>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vint8>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vfloat8>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vbool16>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vint16>::value);
    OIIO_CHECK_ASSERT(std::is_trivially_copyable<vfloat16>::value);
}



int
main(int argc, char* argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif
    for (int i = 0; i < 16; ++i) {
        dummy_float[i] = 1.0f;
        dummy_int[i]   = 1;
    }

    getargs(argc, argv);

    std::string oiiosimd = OIIO::get_string_attribute("oiio:simd");
    std::string hwsimd   = OIIO::get_string_attribute("hw:simd");
    std::cout << "OIIO SIMD support is: " << (oiiosimd.size() ? oiiosimd : "")
              << "\n";
    std::cout << "Hardware SIMD support is: " << (hwsimd.size() ? hwsimd : "")
              << "\n";
    std::cout << "\n";

    Timer timer;

    vint4 dummy4(0);
    vint8 dummy8(0);
    benchmark("null benchmark 4", [](const vint4&) { return int(0); }, dummy4);
    benchmark("null benchmark 8", [](const vint8&) { return int(0); }, dummy8);

    category_heading("vfloat4");
    test_loadstore<vfloat4>();
    test_conversion_loadstore_float<vfloat4>();
    test_masked_loadstore<vfloat4>();
    test_gatherscatter<vfloat4>();
    test_component_access<vfloat4>();
    test_arithmetic<vfloat4>();
    test_comparisons<vfloat4>();
    test_shuffle4<vfloat4>();
    test_swizzle<vfloat4>();
    test_blend<vfloat4>();
    test_transpose4<vfloat4>();
    test_vectorops_vfloat4();
    test_fused<vfloat4>();
    test_mathfuncs<vfloat4>();

    category_heading("vfloat3");
    test_loadstore<vfloat3>();
    test_conversion_loadstore_float<vfloat3>();
    test_component_access<vfloat3>();
    test_arithmetic<vfloat3>();
    // Unnecessary to test these, they just use the vfloat4 ops.
    // test_comparisons<vfloat3> ();
    // test_shuffle4<vfloat3> ();
    // test_swizzle<vfloat3> ();
    // test_blend<vfloat3> ();
    // test_transpose4<vfloat3> ();
    test_vectorops_vfloat3();
    test_fused<vfloat3>();
    // test_mathfuncs<vfloat3>();

    category_heading("vfloat8");
    test_loadstore<vfloat8>();
    test_conversion_loadstore_float<vfloat8>();
    test_masked_loadstore<vfloat8>();
    test_gatherscatter<vfloat8>();
    test_component_access<vfloat8>();
    test_arithmetic<vfloat8>();
    test_comparisons<vfloat8>();
    test_shuffle8<vfloat8>();
    test_blend<vfloat8>();
    test_fused<vfloat8>();
    test_mathfuncs<vfloat8>();

    category_heading("vfloat16");
    test_loadstore<vfloat16>();
    test_conversion_loadstore_float<vfloat16>();
    test_masked_loadstore<vfloat16>();
    test_gatherscatter<vfloat16>();
    test_component_access<vfloat16>();
    test_arithmetic<vfloat16>();
    test_comparisons<vfloat16>();
    test_shuffle16<vfloat16>();
    test_blend<vfloat16>();
    test_fused<vfloat16>();
    test_mathfuncs<vfloat16>();

    category_heading("vint4");
    test_loadstore<vint4>();
    test_conversion_loadstore_int<vint4>();
    test_masked_loadstore<vint4>();
    test_gatherscatter<vint4>();
    test_component_access<vint4>();
    test_arithmetic<vint4>();
    test_bitwise_int<vint4>();
    test_comparisons<vint4>();
    test_shuffle4<vint4>();
    test_blend<vint4>();
    test_vint_to_uint16s<vint4>();
    test_vint_to_uint8s<vint4>();
    test_shift<vint4>();
    test_transpose4<vint4>();

    category_heading("vint8");
    test_loadstore<vint8>();
    test_conversion_loadstore_int<vint8>();
    test_masked_loadstore<vint8>();
    test_gatherscatter<vint8>();
    test_component_access<vint8>();
    test_arithmetic<vint8>();
    test_bitwise_int<vint8>();
    test_comparisons<vint8>();
    test_shuffle8<vint8>();
    test_blend<vint8>();
    test_vint_to_uint16s<vint8>();
    test_vint_to_uint8s<vint8>();
    test_shift<vint8>();

    category_heading("vint16");
    test_loadstore<vint16>();
    test_conversion_loadstore_int<vint16>();
    test_masked_loadstore<vint16>();
    test_gatherscatter<vint16>();
    test_component_access<vint16>();
    test_arithmetic<vint16>();
    test_bitwise_int<vint16>();
    test_comparisons<vint16>();
    test_shuffle16<vint16>();
    test_blend<vint16>();
    test_vint_to_uint16s<vint16>();
    test_vint_to_uint8s<vint16>();
    test_shift<vint16>();

    category_heading("vbool4");
    test_shuffle4<vbool4>();
    test_component_access<vbool4>();
    test_bitwise_bool<vbool4>();

    category_heading("vbool8");
    test_shuffle8<vbool8>();
    test_component_access<vbool8>();
    test_bitwise_bool<vbool8>();

    category_heading("vbool16");
    // test_shuffle16<vbool16> ();
    test_component_access<vbool16>();
    test_bitwise_bool<vbool16>();

    category_heading("Odds and ends");
    test_constants();
    test_special();
    test_metaprogramming();
    test_matrix();
    test_trivially_copyable();

    std::cout << "\nTotal time: " << Strutil::timeintervalformat(timer())
              << "\n";

    return unit_test_failures;
}
