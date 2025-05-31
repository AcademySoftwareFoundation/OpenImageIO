// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/half.h>

#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/unittest.h>

#include "imageio_pvt.h"

#include <iostream>

using namespace OIIO;



template<typename T>
void
test_image_span()
{
    print("testing image_span {}\n",
          TypeDescFromC<std::remove_cv_t<T>>::value());

    const int X = 4, Y = 3, C = 3, Z = 1;
    static T IMG[Z][Y][X][C] = {
        // 4x3 2D image with 3 channels
        { { { 0, 0, 0 }, { 1, 0, 1 }, { 2, 0, 2 }, { 3, 0, 3 } },
          { { 0, 1, 4 }, { 1, 1, 5 }, { 2, 1, 6 }, { 3, 1, 7 } },
          { { 0, 2, 8 }, { 1, 2, 9 }, { 2, 2, 10 }, { 3, 2, 11 } } }
    };

    // Test a 2D image_span
    {
        image2d_span<T> I((T*)IMG, C, X, Y);
        OIIO_CHECK_EQUAL(I.getptr(0, 0, 0), &IMG[0][0][0][0]);
        OIIO_CHECK_EQUAL(I.getptr(1, 0, 0), &IMG[0][0][0][1]);
        OIIO_CHECK_EQUAL(I.getptr(0, 1, 0), &IMG[0][0][1][0]);
        for (int y = 0, i = 0; y < Y; ++y) {
            for (int x = 0; x < X; ++x, ++i) {
                OIIO_CHECK_EQUAL(I.get(0, x, y), x);
                OIIO_CHECK_EQUAL(I.get(1, x, y), y);
                OIIO_CHECK_EQUAL(I.get(2, x, y), i);
                OIIO_CHECK_EQUAL(I(x, y)[0], x);
                OIIO_CHECK_EQUAL(I(x, y)[1], y);
                OIIO_CHECK_EQUAL(I(x, y)[2], i);
            }
        }
    }

    // Test a full volumetric image
    {
        image_span<T> I((T*)IMG, C, X, Y, Z);
        OIIO_CHECK_EQUAL(I.getptr(0, 0, 0), &IMG[0][0][0][0]);
        OIIO_CHECK_EQUAL(I.getptr(1, 0, 0), &IMG[0][0][0][1]);
        OIIO_CHECK_EQUAL(I.getptr(0, 1, 0), &IMG[0][0][1][0]);
        OIIO_CHECK_EQUAL(I.getptr(0, 0, 1), &IMG[0][1][0][0]);
        for (int z = 0; z < Z; ++z) {
            for (int y = 0, i = 0; y < Y; ++y) {
                for (int x = 0; x < X; ++x, ++i) {
                    OIIO_CHECK_EQUAL(I.get(0, x, y, z), x);
                    OIIO_CHECK_EQUAL(I.get(1, x, y, z), y);
                    OIIO_CHECK_EQUAL(I.get(2, x, y, z), i);
                    OIIO_CHECK_EQUAL(I(x, y, z)[0], x);
                    OIIO_CHECK_EQUAL(I(x, y, z)[1], y);
                    OIIO_CHECK_EQUAL(I(x, y, z)[2], i);
                }
            }
        }
    }

    // Extra tests for mutable types
    if constexpr (!std::is_const_v<T>) {
        image_span<T> I((T*)IMG, C, X, Y, Z);
        for (int y = 0, i = 0; y < Y; ++y) {
            for (int x = 0; x < X; ++x, ++i) {
                I(x, y)[0] = x;
                I(x, y)[1] = y;
                I(x, y)[2] = i;
            }
        }

        for (int y = 0, i = 0; y < Y; ++y) {
            for (int x = 0; x < X; ++x, ++i) {
                OIIO_CHECK_EQUAL(I(x, y)[0], x);
                OIIO_CHECK_EQUAL(I(x, y)[1], y);
                OIIO_CHECK_EQUAL(I(x, y)[2], i);
            }
        }
    }
}



template<typename T>
T
testvalue(int x, int y, int z, int c)
{
    return T(x + y + z + c);
}


// Fill an image span in a characteristic way
template<typename T>
void
fill_image_span(image_span<T> img)
{
    // Fill the image with a constant value
    for (uint32_t z = 0; z < img.depth(); ++z) {
        for (uint32_t y = 0; y < img.height(); ++y) {
            for (uint32_t x = 0; x < img.width(); ++x) {
                for (uint32_t c = 0; c < img.nchannels(); ++c) {
                    img(x, y, z)[c] = testvalue<T>(x, y, z, c);
                }
            }
        }
    }
}


// Check that an image span in the characteristic way
template<typename T, typename S = T>
bool
check_image_span(image_span<T> img, int xoff = 0, int yoff = 0, int zoff = 0)
{
    // Fill the image with a constant value
    for (uint32_t z = 0; z < img.depth(); ++z) {
        for (uint32_t y = 0; y < img.height(); ++y) {
            for (uint32_t x = 0; x < img.width(); ++x) {
                for (uint32_t c = 0; c < img.nchannels(); ++c) {
                    auto v = convert_type<S, T>(
                        testvalue<S>(x + xoff, y + yoff, z + zoff, c));
                    OIIO_CHECK_EQUAL(img(x, y, z)[c], v);
                    if (img(x, y, z)[c] != v) {
                        print("\tError at ({}, {}, {})[{}]\n", x, y, z, c);
                        return false;
                    }
                }
            }
        }
    }
    return true;
}



template<typename T = float>
void
test_image_span_copy_image()
{
    const int xres = 2048, yres = 1536, nchans = 4;
    const size_t chansize = sizeof(T);
    print("\nTesting copy_image {} (total {} MB):\n", TypeDescFromC<T>::value(),
          xres * yres * nchans * chansize * 3 / 4 / 1024 / 1024);

    // We test different amounts of contiguity. Each test copies 3/4 of the
    // total image, to keep the total number of bytes copied identical.
    const stride_t src_xstride(chansize * nchans);
    const stride_t src_ystride(src_xstride * xres);
    for (int i = 0; i < 3; ++i) {
        size_t nc(nchans), w(xres), h(yres);
        std::string label;
        if (i == 0) {
            // Fully contiguous region -- copy 3/4 of the image.
            label = "contig buffer";
            h     = h * 3 / 4;
        } else if (i == 1) {
            // Contiguous scanlines -- copy 3/4 of the width of each scanline.
            label = "contig scanlines";
            w     = w * 3 / 4;
        } else if (i == 2) {
            // Contiguous pixels -- copy 3 of 4 channels of each pixel.
            label = "contig pixels";
            nc    = nc * 3 / 4;
        }

        print("  test image_span copy_image {}\n", label);
        std::vector<T> sbuf(xres * yres * nchans);
        std::vector<T> dbuf(w * h * nc);

        // Spans for src and dst -- src has the "original" strides, dst
        // has contiguous strides.
        image_span<T> sispan(sbuf.data(), nc, w, h, 1, chansize, src_xstride,
                             src_ystride, AutoStride);
        image_span<T> dispan(dbuf.data(), nc, w, h, 1);

        // Test correctness
        fill_image_span(sispan);
        copy_image(dispan, sispan);
        OIIO_CHECK_ASSERT(check_image_span(dispan));

        // Benchmark old (ptr) versus new (span) copy_image functions
        Benchmarker bench;
        bench.units(Benchmarker::Unit::us);

        bench(Strutil::format("    copy_image image_span {}", label),
              [&]() { copy_image(dispan, sispan); });
        // Test equivalent version with pointers
        bench(Strutil::format("    copy_image raw ptrs   {}", label), [&]() {
            copy_image(nc, w, h, 1, sbuf.data(), nc * chansize, src_xstride,
                       src_ystride, AutoStride, dbuf.data(), AutoStride,
                       AutoStride, AutoStride);
        });
    }
}



template<typename T = float>
void
test_image_span_contiguize()
{
    // Benchmark old (ptr) versus new (span) contiguize functions
    using pvt::contiguize;

    const int xres = 2048, yres = 1536, nchans = 4;
    const size_t chansize = sizeof(T);
    print("\nTesting contiguize {} (total {} MB):\n", TypeDescFromC<T>::value(),
          xres * yres * nchans * chansize * 3 / 4 / 1024 / 1024);

    // std::vector<T> sbuf(xres * yres * nchans);
    // std::vector<T> dbuf(xres * yres * nchans);

    // We test different amounts of contiguity. Each test copies 3/4 of the
    // total image, to keep the total number of bytes copied identical.
    const stride_t src_xstride(chansize * nchans);
    const stride_t src_ystride(src_xstride * xres);
    for (int i = 0; i < 3; ++i) {
        size_t nc(nchans), /*pixsize(chansize * nchans),*/ w(xres), h(yres);
        std::string label;
        if (i == 0) {
            // Fully contiguous region -- copy 3/4 of the image.
            label = "contig buffer";
            h     = h * 3 / 4;
        } else if (i == 1) {
            // Contiguous scanlines -- copy 3/4 of the width of each scanline.
            label = "contig scanlines";
            w     = w * 3 / 4;
        } else if (i == 2) {
            // Contiguous pixels -- copy 3 of 4 channels of each pixel.
            label = "contig pixels";
            nc    = nc * 3 / 4;
        }

        print("  test image_span contiguize {}\n", label);
        std::vector<T> sbuf(xres * yres * nchans, T(100));
        std::vector<T> dbuf(w * h * nc, T(100));

        // Spans for src and dst -- src has the "original" strides, dst
        // has contiguous strides.
        image_span<T> sispan(sbuf.data(), nc, w, h, 1, chansize, src_xstride,
                             src_ystride, AutoStride);
        image_span<T> dispan(dbuf.data(), nc, w, h, 1);

        // Test correctness
        fill_image_span(sispan);
        auto rspan = contiguize(sispan.as_bytes_image_span(),
                                as_writable_bytes(make_span(dbuf)));
        OIIO_CHECK_ASSERT(check_image_span(
            image_span<const T>(reinterpret_cast<const T*>(rspan.data()), nc, w,
                                h, 1)));

        // Benchmark old (ptr) versus new (span) contiguize functions
        Benchmarker bench;
        bench.units(Benchmarker::Unit::us);

        bench(Strutil::format("    contiguize image_span {}", label), [&]() {
            auto r = contiguize(sispan.as_writable_bytes_image_span(),
                                as_writable_bytes(make_span(dbuf)));
            OIIO_ASSERT(r.size_bytes() == nc * w * h * sizeof(T));
        });
        bench(Strutil::format("    contiguize raw ptrs   {}", label), [&]() {
            contiguize(sbuf.data(), nc, src_xstride, src_ystride,
                       src_ystride * h, dbuf.data(), w, h, 1,
                       TypeDescFromC<T>::value());
        });
    }
}



template<typename Stype = float, typename Dtype = Stype>
void
test_image_span_convert_image()
{
    // Benchmark old (ptr) versus new (span) convert_image functions
    const int xres = 2048, yres = 1536, nchans = 4;
    const size_t schansize = sizeof(Stype);
    const size_t dchansize = sizeof(Dtype);
    print("\nTesting convert_image {} -> {} (total {}M values):\n",
          TypeDescFromC_v<Stype>, TypeDescFromC_v<Dtype>,
          xres * yres * nchans * 3 / 4 / 1024 / 1024);

    // We test different amounts of contiguity. Each test copies 3/4 of the
    // total image, to keep the total number of bytes copied identical.
    const stride_t src_xstride(schansize * nchans);
    const stride_t src_ystride(src_xstride * xres);
    const stride_t dst_xstride(dchansize * nchans);
    const stride_t dst_ystride(dst_xstride * xres);
    for (int i = 0; i < 3; ++i) {
        size_t nc(nchans), w(xres), h(yres);
        std::string label;
        if (i == 0) {
            // Fully contiguous region -- copy 3/4 of the image.
            label = "contig buffer";
            h     = h * 3 / 4;
        } else if (i == 1) {
            // Contiguous scanlines -- copy 3/4 of the width of each scanline.
            label = "contig scanlines";
            w     = w * 3 / 4;
        } else if (i == 2) {
            // Contiguous pixels -- copy 3 of 4 channels of each pixel.
            label = "contig pixels";
            nc    = nc * 3 / 4;
        }

        print("  test convert_image {}\n", label);
        std::vector<Stype> sbuf(xres * yres * nchans, Stype(10));
        std::vector<Dtype> dbuf(xres * yres * nchans, Dtype(20));

        // Spans for src and dst
        image_span sispan(sbuf.data(), nc, w, h, 1, schansize, src_xstride,
                          src_ystride, AutoStride);
        image_span dispan(dbuf.data(), nc, w, h, 1, dchansize, dst_xstride,
                          dst_ystride, AutoStride);

        fill_image_span(sispan);

        // Benchmark old (ptr) versus new (span) contiguize functions
        Benchmarker bench;
        bench.units(Benchmarker::Unit::ms);

        bench(Strutil::format("    convert_image image_span {}", label),
              [&]() { convert_image(sispan, dispan); });
        // Test correctness
        bench(Strutil::format("    convert_image raw ptrs   {}", label), [&]() {
            convert_image(nc, w, h, 1, sbuf.data(), TypeDescFromC_v<Stype>,
                          src_xstride, src_ystride, AutoStride, dbuf.data(),
                          TypeDescFromC_v<Dtype>, dst_xstride, dst_ystride,
                          AutoStride);
        });
        OIIO_CHECK_ASSERT((check_image_span<Dtype, Stype>(dispan)));
    }
}



// Sum all values in an image using a pass-by-value image_span
float
sum_image_span_val(image_span<const float> img)
{
    float sum = 0;
    for (uint32_t z = 0; z < img.depth(); ++z) {
        for (uint32_t y = 0; y < img.height(); ++y) {
            for (uint32_t x = 0; x < img.width(); ++x) {
                for (uint32_t c = 0; c < img.nchannels(); ++c) {
                    sum += img.get(c, x, y, z);
                }
            }
        }
    }
    return sum;
}


// Sum all values in an image using a pass-by-reference image_span
float
sum_image_span_ref(const image_span<const float>& img)
{
    float sum = 0;
    for (uint32_t z = 0; z < img.depth(); ++z) {
        for (uint32_t y = 0; y < img.height(); ++y) {
            for (uint32_t x = 0; x < img.width(); ++x) {
                for (uint32_t c = 0; c < img.nchannels(); ++c) {
                    sum += img.get(c, x, y, z);
                }
            }
        }
    }
    return sum;
}


// Sum all values in an image using raw pointers, sizes, strides
float
sum_image_span_ptr(const float* ptr, uint32_t chans, uint32_t width,
                   uint32_t height, uint32_t depth, int64_t chstride,
                   int64_t xstride, int64_t ystride, int64_t zstride)
{
    float sum = 0;
    for (uint32_t z = 0; z < depth; ++z) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                for (uint32_t c = 0; c < chans; ++c) {
                    const float* p = reinterpret_cast<const float*>(
                        (const char*)ptr + c * chstride + x * xstride
                        + y * ystride + z * zstride);
                    sum += *p;
                }
            }
        }
    }
    return sum;
}



void
benchmark_image_span_passing()
{
    print("\nbenchmark_image_span_passing\n");
    const int xres = 2048, yres = 1536, nchans = 4;
    std::vector<float> sbuf(xres * yres * nchans, 1.0f);
    image_span<const float> ispan(sbuf.data(), nchans, xres, yres, 1);

    Benchmarker bench;
    bench.units(Benchmarker::Unit::us);
    float sum = 0.0f;

    bench("  pass by value     (big)",
          [=, &sum]() { sum += sum_image_span_val(ispan); });
    bench("  pass by value imm (big)", [=, &sum]() {
        sum += sum_image_span_val(
            image_span<const float>(sbuf.data(), nchans, xres, yres, 1));
    });
    bench("  pass by ref       (big)",
          [=, &sum]() { sum += sum_image_span_ref(ispan); });
    bench("  pass by ref imm   (big)", [=, &sum]() {
        sum += sum_image_span_ref(
            image_span<const float>(sbuf.data(), nchans, xres, yres, 1));
    });
    bench("  pass by ptr       (big)", [=, &sum]() {
        sum += sum_image_span_ptr(sbuf.data(), nchans, xres, yres, 1,
                                  sizeof(float), nchans * sizeof(float),
                                  nchans * sizeof(float) * xres,
                                  nchans * sizeof(float) * xres * yres);
    });

    // Do it all again for a SMALL image
    bench.units(Benchmarker::Unit::ns);
    int small = 16;
    image_span<const float> smispan(sbuf.data(), nchans, small, small, 1);
    bench("  pass by value     (small)",
          [=, &sum]() { sum += sum_image_span_val(smispan); });
    bench("  pass by value imm (small)", [=, &sum]() {
        sum += sum_image_span_val(
            image_span<const float>(sbuf.data(), nchans, small, small, 1));
    });
    bench("  pass by ref       (small)",
          [=, &sum]() { sum += sum_image_span_ref(smispan); });
    bench("  pass by ref imm   (small)", [=, &sum]() {
        sum += sum_image_span_ref(
            image_span<const float>(sbuf.data(), nchans, small, small, 1));
    });
    bench("  pass by ptr       (small)", [=, &sum]() {
        sum += sum_image_span_ptr(sbuf.data(), nchans, small, small, 1,
                                  sizeof(float), nchans * sizeof(float),
                                  nchans * sizeof(float) * small,
                                  nchans * sizeof(float) * small * small);
    });
    print("  [sum={}]\n", sum);  // seems necessary to not optimize away
}



void
test_image_span_within_span()
{
    print("\ntest_image_span_within_span\n");

    const int nchans = 3, xres = 5, yres = 7, zres = 11;
    const int chstride = sizeof(float), xstride = chstride * nchans,
              ystride = xstride * xres, zstride = ystride * yres;
    float array[nchans * xres * yres * zres];
    cspan<float> aspan(array);
    // It better worrk with the same origin and default strides
    OIIO_CHECK_ASSERT(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, xstride, ystride, zstride),
                               aspan));
    // Make sure too big are recognized
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride + 1, xstride, ystride,
                                          zstride),
                               aspan));
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, xstride + 1, ystride,
                                          zstride),
                               aspan));
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, xstride, ystride + 1,
                                          zstride),
                               aspan));
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, xstride, ystride,
                                          zstride + 1),
                               aspan));
    // Make sure negagive strides used CORRECTLY are recognized
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          -chstride, xstride, ystride, zstride),
                               aspan));
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, -xstride, ystride, zstride),
                               aspan));
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, xstride, -ystride, zstride),
                               aspan));
    OIIO_CHECK_FALSE(
        image_span_within_span(image_span(array, nchans, xres, yres, zres,
                                          chstride, xstride, ystride, -zstride),
                               aspan));
    // Make sure negagive strides used CORRECTLY are recognized
    OIIO_CHECK_ASSERT(
        image_span_within_span(image_span(array + nchans - 1, nchans, xres,
                                          yres, zres, -chstride, xstride,
                                          ystride, zstride),
                               aspan));
    OIIO_CHECK_ASSERT(
        image_span_within_span(image_span(array + (xres - 1) * nchans, nchans,
                                          xres, yres, zres, chstride, -xstride,
                                          ystride, zstride),
                               aspan));
    OIIO_CHECK_ASSERT(
        image_span_within_span(image_span(array + (yres - 1) * xres * nchans,
                                          nchans, xres, yres, zres, chstride,
                                          xstride, -ystride, zstride),
                               aspan));
    OIIO_CHECK_ASSERT(image_span_within_span(
        image_span(array + (zres - 1) * xres * yres * nchans, nchans, xres,
                   yres, zres, chstride, xstride, ystride, -zstride),
        aspan));
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_image_span<float>();
    test_image_span<const float>();
    test_image_span<uint16_t>();
    test_image_span<const uint16_t>();
    test_image_span<uint8_t>();
    test_image_span<const uint8_t>();

    test_image_span_copy_image<float>();
    test_image_span_copy_image<uint16_t>();
    test_image_span_copy_image<uint8_t>();

    test_image_span_contiguize<float>();
    test_image_span_contiguize<uint16_t>();
    test_image_span_contiguize<uint8_t>();

    test_image_span_convert_image<float, half>();
    test_image_span_convert_image<float, uint16_t>();
    test_image_span_convert_image<float, uint8_t>();
    test_image_span_convert_image<half, float>();
    test_image_span_convert_image<uint16_t, float>();
    test_image_span_convert_image<uint8_t, float>();
    test_image_span_convert_image<uint16_t, uint8_t>();
    test_image_span_convert_image<uint8_t, uint16_t>();
    test_image_span_convert_image<uint16_t, half>();
    test_image_span_convert_image<half, uint16_t>();

    test_image_span_within_span();

    benchmark_image_span_passing();

    return unit_test_failures;
}
