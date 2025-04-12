// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


#include <OpenImageIO/benchmark.h>
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



template<typename T = float>
void
benchmark_image_span_copy_image()
{
    // Benchmark old (ptr) versus new (span) copy_image functions
    Benchmarker bench;
    bench.units(Benchmarker::Unit::us);
    const int xres = 2048, yres = 1536, nchans = 4;
    std::vector<T> sbuf(xres * yres * nchans);
    std::vector<T> dbuf(xres * yres * nchans);
    const size_t chansize = sizeof(T);

    print("Benchmarking copy_image {} (total {} MB):\n",
          TypeDescFromC<T>::value(),
          xres * yres * nchans * chansize * 3 / 4 / 1024 / 1024);

    // We test different amounts of contiguity. Each test copies 3/4 of the
    // total image, to keep the total number of bytes copied identical.
    const stride_t src_xstride(chansize * nchans);
    const stride_t src_ystride(src_xstride * xres);
    for (int i = 0; i < 3; ++i) {
        size_t nc(nchans), pixsize(chansize * nchans), w(xres), h(yres);
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
        bench(Strutil::format("  copy_image image_span {}", label), [&]() {
            copy_image(image_span<T>(dbuf.data(), nc, w, h, 1, chansize,
                                     src_xstride, src_ystride, AutoStride,
                                     chansize),
                       image_span<const T>(sbuf.data(), nc, w, h, 1));
        });
        bench(Strutil::format("  copy_image raw ptrs   {}", label), [&]() {
            copy_image(nc, w, h, 1, sbuf.data(), pixsize, src_xstride,
                       src_ystride, AutoStride, dbuf.data(), AutoStride,
                       AutoStride, AutoStride);
        });
    }
}



template<typename T = float>
void
benchmark_image_span_contiguize()
{
    // Benchmark old (ptr) versus new (span) contiguize functions
    using pvt::contiguize;

    // Benchmark old (ptr) versus new (span) contiguize functions
    Benchmarker bench;
    bench.units(Benchmarker::Unit::us);
    const int xres = 2048, yres = 1536, nchans = 4;
    std::vector<T> sbuf(xres * yres * nchans);
    std::vector<T> dbuf(xres * yres * nchans);
    const size_t chansize = sizeof(T);

    print("Benchmarking contiguize {} (total {} MB):\n",
          TypeDescFromC<T>::value(),
          xres * yres * nchans * chansize * 3 / 4 / 1024 / 1024);

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
        bench(Strutil::format("  contiguize image_span {}", label), [&]() {
            auto r = contiguize(image_span(reinterpret_cast<const std::byte*>(
                                               sbuf.data()),
                                           nc, w, h, 1, chansize, src_xstride,
                                           src_ystride, AutoStride, chansize),
                                as_writable_bytes(make_span(dbuf)));
            OIIO_ASSERT(r.size_bytes() == nc * w * h * sizeof(T));
        });
        bench(Strutil::format("  contiguize raw ptrs   {}", label), [&]() {
            contiguize(sbuf.data(), nc, src_xstride, src_ystride,
                       src_ystride * h, dbuf.data(), w, h, 1,
                       TypeDescFromC<T>::value());
        });
    }
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

    benchmark_image_span_copy_image<float>();
    benchmark_image_span_copy_image<uint16_t>();
    benchmark_image_span_copy_image<uint8_t>();

    benchmark_image_span_contiguize<float>();
    benchmark_image_span_contiguize<uint16_t>();
    benchmark_image_span_contiguize<uint8_t>();

    return unit_test_failures;
}
