// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/strutil.h>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace OIIO;

static int iterations = 10;
static int width = 2048;
static int height = 2048;

struct BenchResult {
    std::string type;
    std::string op;
    double time_ms;
};

std::vector<BenchResult> results;

template<typename T>
void run_benchmark(TypeDesc format, const std::string& type_name) {
    ImageSpec spec(width, height, 3, format);
    ImageBuf A(spec);
    ImageBuf B(spec);
    ImageBuf R(spec);

    // Setup Pattern A (Contrast 0.4 - 0.8)
    float colorA1[] = { 0.4f, 0.4f, 0.4f };
    float colorA2[] = { 0.8f, 0.8f, 0.8f };
    ImageBufAlgo::checker(A, 64, 64, 1, colorA1, colorA2);

    // Setup Pattern B (Range 0.2 - 0.4)
    float colorB1[] = { 0.2f, 0.2f, 0.2f };
    float colorB2[] = { 0.4f, 0.4f, 0.4f };
    ImageBufAlgo::checker(B, 32, 32, 1, colorB1, colorB2);

    // Ensure memory is allocated
    A.read(0, 0, true, format);
    B.read(0, 0, true, format);
    
    // Bench ADD
    {
        Timer t;
        for (int i = 0; i < iterations; ++i) {
            ImageBufAlgo::add(R, A, B);
        }
        results.push_back({type_name, "add", t() * 1000.0 / iterations});
    }

    // Bench SUB
    {
        Timer t;
        for (int i = 0; i < iterations; ++i) {
            ImageBufAlgo::sub(R, A, B);
        }
        results.push_back({type_name, "sub", t() * 1000.0 / iterations});
    }

    // Bench MUL
    {
        Timer t;
        for (int i = 0; i < iterations; ++i) {
            ImageBufAlgo::mul(R, A, B);
        }
        results.push_back({type_name, "mul", t() * 1000.0 / iterations});
    }

    // Bench POW (2.2)
    {
        Timer t;
        for (int i = 0; i < iterations; ++i) {
            ImageBufAlgo::pow(R, A, 2.2f);
        }
        results.push_back({type_name, "pow(2.2)", t() * 1000.0 / iterations});
    }
}

int main(int argc, char** argv) {
    std::cout << "Benchmarking OIIO Arithmetic Operations" << std::endl;
    std::cout << "Image Size: " << width << "x" << height << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    run_benchmark<uint8_t>(TypeDesc::UINT8, "uint8");
    run_benchmark<uint16_t>(TypeDesc::UINT16, "uint16");
    run_benchmark<uint32_t>(TypeDesc::UINT32, "uint32");
    run_benchmark<half>(TypeDesc::HALF, "half");
    run_benchmark<float>(TypeDesc::FLOAT, "float");
    run_benchmark<double>(TypeDesc::DOUBLE, "double");

    // Output Table
    std::cout << "\nResults (Average ms):\n";
    std::cout << "| Type       | Operation  | Time (ms) |\n";
    std::cout << "|:-----------|:-----------|----------:|\n";
    
    for (const auto& res : results) {
        std::cout << "| " << std::left << std::setw(10) << res.type 
                  << " | " << std::setw(10) << res.op 
                  << " | " << std::right << std::setw(8) << std::fixed << std::setprecision(3) << res.time_ms 
                  << " |\n";
    }

    return 0;
}
