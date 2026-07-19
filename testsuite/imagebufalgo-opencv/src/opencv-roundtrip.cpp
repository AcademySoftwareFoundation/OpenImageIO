// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// Minimal, standalone exercise of imagebufalgo_opencv.h: build a small
// gradient ImageBuf, round-trip it through cv::Mat via to_OpenCV() /
// from_OpenCV(), and verify the pixels survive unchanged. This is meant to
// smoke-test that the header still compiles and links correctly against
// whatever OpenCV version is installed (the header does version-dependent
// things internally based on OIIO_OPENCV_VERSION).

#include <opencv2/opencv.hpp>

#include <OpenImageIO/imagebufalgo_opencv.h>

using namespace OIIO;


int
main()
{
    ImageBuf src
        = ImageBufAlgo::fill({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
                             { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
                             ROI(0, 64, 0, 64, 0, 1, 0, 3));

    cv::Mat mat;
    bool to_ok = ImageBufAlgo::to_OpenCV(mat, src);
    if (to_ok && !mat.empty()) {
        OIIO::print("to_OpenCV: PASS\n");
    } else {
        OIIO::print("to_OpenCV FAIL: {}\n", OIIO::geterror());
        return 1;
    }

    ImageBuf dst = ImageBufAlgo::from_OpenCV(mat);
    if (!dst.has_error()) {
        OIIO::print("from_OpenCV: PASS\n");
    } else {
        OIIO::print("from_OpenCV FAIL: {}\n", dst.geterror());
        return 1;
    }

    auto comp         = ImageBufAlgo::compare(src, dst, 0.0f, 0.0f);
    bool roundtrip_ok = !comp.error && comp.maxerror == 0.0f;
    OIIO::print("round trip: {}\n", roundtrip_ok ? "PASS" : "FAIL");

    return roundtrip_ok ? 0 : 1;
}
