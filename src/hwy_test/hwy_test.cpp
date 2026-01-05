// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

/// Benchmark Highway SIMD vs Scalar implementations
/// Compares performance by toggling OIIO::attribute("enable_hwy", 0/1)

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/timer.h>

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace OIIO;

struct BenchResult {
    double scalar_ms;
    double simd_ms;
    double speedup;
};

// Run a benchmark function multiple times and return average time in milliseconds
template<typename Func>
double
benchmark_ms(Func&& func, int iterations = 100, int warmup = 5)
{
    // Warmup
    for (int i = 0; i < warmup; ++i) {
        func();
    }

    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    return timer() * 1000.0 / iterations;  // Convert to ms
}

// Benchmark add operation
BenchResult
bench_add(const ImageBuf& A, const ImageBuf& B, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    // Scalar version
    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::add(R, A, B); },
                                    iterations);

    // SIMD version
    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::add(R, A, B); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark sub operation
BenchResult
bench_sub(const ImageBuf& A, const ImageBuf& B, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::sub(R, A, B); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::sub(R, A, B); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark mul operation
BenchResult
bench_mul(const ImageBuf& A, const ImageBuf& B, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::mul(R, A, B); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::mul(R, A, B); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark pow operation
BenchResult
bench_pow(const ImageBuf& A, cspan<float> exponent, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::pow(R, A, exponent); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::pow(R, A, exponent); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark rangecompress operation
BenchResult
bench_rangecompress(const ImageBuf& A, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::rangecompress(R, A); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::rangecompress(R, A); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark rangeexpand operation
BenchResult
bench_rangeexpand(const ImageBuf& A, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::rangeexpand(R, A); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::rangeexpand(R, A); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark premult operation
BenchResult
bench_premult(const ImageBuf& A, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::premult(R, A); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::premult(R, A); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark unpremult operation
BenchResult
bench_unpremult(const ImageBuf& A, int iterations = 100)
{
    BenchResult result;
    ImageBuf R(A.spec());

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::unpremult(R, A); },
                                    iterations);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::unpremult(R, A); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;
    return result;
}

// Benchmark resample operation
BenchResult
bench_resample(const ImageBuf& A, int new_width, int new_height,
               int iterations = 50)
{
    BenchResult result;
    ImageSpec newspec = A.spec();
    newspec.width     = new_width;
    newspec.height    = new_height;

    // Scalar version - ensure proper allocation
    ImageBuf R_scalar(newspec);
    ImageBufAlgo::zero(R_scalar);  // Ensure buffer is allocated!

    OIIO::attribute("enable_hwy", 0);
    result.scalar_ms
        = benchmark_ms([&]() { ImageBufAlgo::resample(R_scalar, A); },
                       iterations);

    // SIMD version
    ImageBuf R_simd(newspec);
    ImageBufAlgo::zero(R_simd);

    OIIO::attribute("enable_hwy", 1);
    result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::resample(R_simd, A); },
                                  iterations);

    result.speedup = result.scalar_ms / result.simd_ms;

    // Validate results - check for differences
    auto comp = ImageBufAlgo::compare(R_scalar, R_simd, 0.001f, 0.001f);
    if (comp.maxerror > 0.001f) {
        printf("    \033[33m[INFO] max error: %.6f at (%d, %d, c%d)\033[0m\n",
               comp.maxerror, comp.maxx, comp.maxy, comp.maxc);

        // Print actual pixel values at the error location
        std::vector<float> scalar_pixel(R_scalar.nchannels());
        std::vector<float> simd_pixel(R_simd.nchannels());
        R_scalar.getpixel(comp.maxx, comp.maxy, scalar_pixel.data());
        R_simd.getpixel(comp.maxx, comp.maxy, simd_pixel.data());
        printf("    Scalar ch%d: %.6f, SIMD ch%d: %.6f, diff: %.6f\n",
               comp.maxc, scalar_pixel[comp.maxc], comp.maxc,
               simd_pixel[comp.maxc],
               std::abs(scalar_pixel[comp.maxc] - simd_pixel[comp.maxc]));
    }

    return result;
}

// Print results
void
print_result(const char* type_name, const BenchResult& result)
{
    const char* color = result.speedup > 1.0 ? "\033[32m" : "\033[31m";
    const char* reset = "\033[0m";
    printf("%-10s | %10.2f | %10.2f | %s%6.2fx%s\n", type_name,
           result.scalar_ms, result.simd_ms, color, result.speedup, reset);
}

void
print_header()
{
    printf("%-10s | %10s | %10s | %-8s\n", "Type", "Scalar(ms)", "SIMD(ms)",
           "Speedup");
    printf("----------------------------------------------------\n");
}

// Get appropriate file extension for type
const char*
get_extension(TypeDesc format)
{
    if (format == TypeDesc::HALF)
        return ".exr";
    return ".tif";
}

// Save image with appropriate format
void
save_image(const ImageBuf& buf, const char* basename, const char* type_name)
{
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_%s%s", basename, type_name,
             get_extension(buf.spec().format));
    if (!buf.write(filename)) {
        printf("    Warning: Failed to save %s\n", filename);
    }
}

// Create test images
ImageBuf
create_test_image(int width, int height, int nchannels, TypeDesc format)
{
    ImageSpec spec(width, height, nchannels, format);
    ImageBuf buf(spec);

    // Create a gradient to ensure meaningful resampling
    std::vector<float> tl(nchannels), tr(nchannels), bl(nchannels),
        br(nchannels);
    for (int c = 0; c < nchannels; ++c) {
        tl[c] = 0.0f;
        tr[c] = 1.0f;
        bl[c] = 0.5f;
        br[c] = 0.0f;
        if (c % 2 == 1) {  // Vary channels
            tl[c] = 1.0f;
            tr[c] = 0.0f;
            bl[c] = 0.0f;
            br[c] = 1.0f;
        }
    }
    ImageBufAlgo::fill(buf, tl, tr, bl, br);
    return buf;
}

ImageBuf
create_checkerboard_image(int width, int height, int nchannels, TypeDesc format,
                          int checker_size = 64)
{
    ImageSpec spec(width, height, nchannels, format);
    ImageBuf buf(spec);

    // Fill with checkerboard pattern
    ImageBufAlgo::checker(buf, checker_size, checker_size, nchannels,
                          { 0.1f, 0.1f, 0.1f }, { 0.9f, 0.9f, 0.9f },
                          0, 0, 0);
    return buf;
}

ImageBuf
create_rgba_image(int width, int height, TypeDesc format)
{
    ImageSpec spec(width, height, 4, format);
    spec.alpha_channel = 3;
    ImageBuf buf(spec);
    // Fill with semi-transparent colors
    ImageBufAlgo::fill(buf, { 0.8f, 0.6f, 0.4f, 0.7f });
    return buf;
}

int
main(int argc, char* argv[])
{
    // Default parameters
    int width      = 2048;
    int height     = 2048;
    int iterations = 20;

    // Parse command line args
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &width, &height) != 2) {
                fprintf(stderr,
                        "Invalid size format. Use WxH (e.g., 2048x2048)\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --size WxH         Image size (default: 2048x2048)\n");
            printf("  --iterations N     Number of iterations (default: 20)\n");
            printf("  --help             Show this help\n");
            return 0;
        }
    }

    printf("Highway SIMD Benchmark\n");
    printf("======================\n");
    printf("Image size: %dx%d\n", width, height);
    printf("Iterations: %d\n", iterations);

    // Verify enable_hwy attribute works
    int hwy_enabled = 0;
    OIIO::getattribute("enable_hwy", hwy_enabled);
    printf("Initial enable_hwy: %d\n", hwy_enabled);

    // Test types
    struct TestConfig {
        const char* name;
        TypeDesc format;
    };

    std::vector<TestConfig> configs = {
        { "uint8", TypeDesc::UINT8 },   { "uint16", TypeDesc::UINT16 },
        { "uint32", TypeDesc::UINT32 }, { "float", TypeDesc::FLOAT },
        { "half", TypeDesc::HALF },     { "double", TypeDesc::DOUBLE },
    };

    // Add
    printf("\n[ Add ]\n");
    print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_add(A, B, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::add(R, A, B);
        save_image(A, "src_A", cfg.name);
        save_image(B, "src_B", cfg.name);
        save_image(R, "result_add", cfg.name);
    }

    // Sub
    printf("\n[ Sub ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_sub(A, B, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::sub(R, A, B);
        save_image(R, "result_sub", cfg.name);
    }

    // Mul
    printf("\n[ Mul ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_mul(A, B, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::mul(R, A, B);
        save_image(R, "result_mul", cfg.name);
    }

    // Pow
    printf("\n[ Pow ]\n");
    //print_header();
    float exponent_vals[] = { 2.2f, 2.2f, 2.2f };
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_pow(A, exponent_vals, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::pow(R, A, exponent_vals);
        save_image(R, "result_pow", cfg.name);
    }


    // Div
    printf("\n[ Div ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_div = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::div(R, A, B); }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::div(R, A, B); }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_div(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::div(R, A, B);
        save_image(R, "result_div", cfg.name);
    }

    // Min
    printf("\n[ Min ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_min = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::min(R, A, B); }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::min(R, A, B); }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_min(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::min(R, A, B);
        save_image(R, "result_min", cfg.name);
    }

    // Max
    printf("\n[ Max ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_max = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::max(R, A, B); }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::max(R, A, B); }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_max(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::max(R, A, B);
        save_image(R, "result_max", cfg.name);
    }

    // Abs
    printf("\n[ Abs ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_abs = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::abs(R, A); }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::abs(R, A); }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_abs(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::abs(R, A);
        save_image(R, "result_abs", cfg.name);
    }

    // Absdiff
    printf("\n[ Absdiff ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_absdiff = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::absdiff(R, A, B); }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::absdiff(R, A, B); }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_absdiff(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::absdiff(R, A, B);
        save_image(R, "result_absdiff", cfg.name);
    }

    // MAD
    printf("\n[ MAD ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf B = create_test_image(width, height, 3, cfg.format);
        ImageBuf C = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_mad = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() { ImageBufAlgo::mad(R, A, B, C); }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() { ImageBufAlgo::mad(R, A, B, C); }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_mad(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::mad(R, A, B, C);
        save_image(R, "result_mad", cfg.name);
    }

    // Clamp
    printf("\n[ Clamp ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        auto bench_clamp = [&](int iters = 100) {
            BenchResult result;
            OIIO::attribute("enable_hwy", 0);
            result.scalar_ms = benchmark_ms([&]() {
                ImageBufAlgo::clamp(R, A, 0.1f, 0.9f);
            }, iters);
            OIIO::attribute("enable_hwy", 1);
            result.simd_ms = benchmark_ms([&]() {
                ImageBufAlgo::clamp(R, A, 0.1f, 0.9f);
            }, iters);
            result.speedup = result.scalar_ms / result.simd_ms;
            return result;
        };

        print_result(cfg.name, bench_clamp(iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::clamp(R, A, 0.1f, 0.9f);
        save_image(R, "result_clamp", cfg.name);
    }

    // RangeCompress
    printf("\n[ RangeCompress ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_rangecompress(A, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::rangecompress(R, A);
        save_image(R, "result_rangecompress", cfg.name);
    }

    // RangeExpand
    printf("\n[ RangeExpand ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_test_image(width, height, 3, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_rangeexpand(A, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::rangeexpand(R, A);
        save_image(R, "result_rangeexpand", cfg.name);
    }

    // Premult
    printf("\n[ Premult ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_rgba_image(width, height, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_premult(A, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::premult(R, A);
        save_image(A, "src_RGBA", cfg.name);
        save_image(R, "result_premult", cfg.name);
    }

    // Unpremult
    printf("\n[ Unpremult ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_rgba_image(width, height, cfg.format);
        ImageBuf R(A.spec());

        print_result(cfg.name, bench_unpremult(A, iterations));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::unpremult(R, A);
        save_image(R, "result_unpremult", cfg.name);
    }


    // Resample 75%
    printf("\n[ Resample 75%% ]\n");
    //print_header();
    int resample_iters = std::max(1, iterations / 2);
    for (const auto& cfg : configs) {
        ImageBuf A = create_checkerboard_image(width, height, 3, cfg.format);
        ImageSpec newspec = A.spec();
        newspec.width = width * 3 / 4;
        newspec.height = height * 3 / 4;

        // Create separate buffers for scalar and SIMD
        ImageBuf R_scalar(newspec);
        ImageBuf R_simd(newspec);
        ImageBufAlgo::zero(R_scalar);
        ImageBufAlgo::zero(R_simd);

        print_result(cfg.name, bench_resample(A, width * 3 / 4, height * 3 / 4,
                                              resample_iters));

        // Save both scalar and SIMD results for comparison
        OIIO::attribute("enable_hwy", 0);
        ImageBufAlgo::resample(R_scalar, A);
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::resample(R_simd, A);

        save_image(A, "src_checkerboard", cfg.name);
        save_image(R_scalar, "result_resample75_scalar", cfg.name);
        save_image(R_simd, "result_resample75_simd", cfg.name);
    }

    // Resample 50%
    printf("\n[ Resample 50%% ]\n");
    //print_header();
    for (const auto& cfg : configs) {
        ImageBuf A = create_checkerboard_image(width, height, 3, cfg.format);
        ImageSpec newspec = A.spec();
        newspec.width = width / 2;
        newspec.height = height / 2;
        ImageBuf R(newspec);
        ImageBufAlgo::zero(R);

        print_result(cfg.name,
                     bench_resample(A, width / 2, height / 2, resample_iters));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::resample(R, A);
        save_image(R, "result_resample50", cfg.name);
    }

    // Resample 25%
    printf("\n[ Resample 25%% ]\n");
    for (const auto& cfg : configs) {
        ImageBuf A = create_checkerboard_image(width, height, 3, cfg.format);
        ImageSpec newspec = A.spec();
        newspec.width = width / 4;
        newspec.height = height / 4;
        ImageBuf R(newspec);
        ImageBufAlgo::zero(R);

        print_result(cfg.name,
                     bench_resample(A, width / 4, height / 4, resample_iters));

        // Save final result
        OIIO::attribute("enable_hwy", 1);
        ImageBufAlgo::resample(R, A);
        save_image(R, "result_resample25", cfg.name);
    }
    print_header();

    printf("\nBenchmark complete!\n");
    printf("Note: Speedup > 1.0x means SIMD is faster (shown in green)\n");
    printf("      Speedup < 1.0x means scalar is faster (shown in red)\n");

    return 0;
}
