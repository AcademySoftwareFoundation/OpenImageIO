// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO


/// \file
/// Implementation of ImageBufAlgo algorithms.

#include <cmath>
#include <iostream> 

#include <OpenImageIO/Imath.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebufalgo_util.h>


using Imath::Color3f;

#include "imageio_pvt.h"



 

template<class T>
inline Imath::Vec3<T>
powf(const Imath::Vec3<T>& x, float y)
{
    return Imath::Vec3<T>(powf(x[0], y), powf(x[1], y), powf(x[2], y));
}



OIIO_NAMESPACE_BEGIN




ImageBufAlgo::CompareResults
ImageBufAlgo::compare_flip(const ImageBuf& A, const ImageBuf& B)
{
    pvt::LoggedTimer logtimer("IBA::compare_flip");
    ImageBufAlgo::CompareResults result;
    // result.error = true;
    // // print(A.name());
    // // // FLIP::image<float> errorMapFLIPOutput;

    // evaluate(A.name(), B.name());


    // // char** args = ["--reference", A.name(), "--test", B.name()]

    // // commandline commandLine = commandline(4, args, getAllowedCommandLineOptions());

    
    // // FLIPTool::execute(commandLine);
    

    print("\nFLIP!!\n");
    print(A.name());
    print(B.name());
    return result;
}



// bool ldrLoad(const std::string& filename, int& imgWidth, int& imgHeight, float*& pixels)
// {
//     int bpp;
//     unsigned char* ldrPixels = stbi_load(filename.c_str(), &imgWidth, &imgHeight, &bpp, 3);
//     if (!ldrPixels)
//     {
//         return false;
//     }

//     pixels = new float[3 * imgWidth * imgHeight];
//     for (int y = 0; y < imgHeight; y++)
//     {
//         for (int x = 0; x < imgWidth; x++)
//         {
//             int linearIdx = 3 * (y * imgWidth + x);
//             pixels[linearIdx + 0] = ldrPixels[linearIdx + 0] / 255.0f;
//             pixels[linearIdx + 1] = ldrPixels[linearIdx + 1] / 255.0f;
//             pixels[linearIdx + 2] = ldrPixels[linearIdx + 2] / 255.0f;

//         }
//     }
//     delete[] ldrPixels;
//     return true;
// }

// bool hdrLoad(const std::string& fileName, int& imgWidth, int& imgHeight, float*& hdrPixels)
// {
//     EXRVersion exrVersion;
//     EXRImage exrImage;
//     EXRHeader exrHeader;
//     InitEXRHeader(&exrHeader);
//     InitEXRImage(&exrImage);

//     {
//         int ret;
//         const char* errorString;

//         ret = ParseEXRVersionFromFile(&exrVersion, fileName.c_str());
//         if (ret != TINYEXR_SUCCESS || exrVersion.multipart || exrVersion.non_image)
//         {
//             std::cerr << "Unsupported EXR version or type!" << std::endl;
//             return false;
//         }

//         ret = ParseEXRHeaderFromFile(&exrHeader, &exrVersion, fileName.c_str(), &errorString);
//         if (ret != TINYEXR_SUCCESS)
//         {
//             std::cerr << "Error loading EXR header: " << errorString << std::endl;
//             return false;
//         }

//         for (int i = 0; i < exrHeader.num_channels; i++)
//         {
//             if (exrHeader.pixel_types[i] == TINYEXR_PIXELTYPE_HALF)
//             {
//                 exrHeader.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
//             }
//         }

//         ret = LoadEXRImageFromFile(&exrImage, &exrHeader, fileName.c_str(), &errorString);
//         if (ret != TINYEXR_SUCCESS)
//         {
//             std::cerr << "Error loading EXR file: " << errorString << std::endl;
//             return false;
//         }
//     }

//     imgWidth = exrImage.width;
//     imgHeight = exrImage.height;

//     int idxR = -1;
//     int idxG = -1;
//     int idxB = -1;
//     int numRecognizedChannels = 0;
//     for (int c = 0; c < exrHeader.num_channels; c++)
//     {
//         std::string channelName = exrHeader.channels[c].name;
//         std::transform(channelName.begin(), channelName.end(), channelName.begin(), ::tolower);
//         if (channelName == "r")
//         {
//             idxR = c;
//             ++numRecognizedChannels;
//         }
//         else if (channelName == "g")
//         {
//             idxG = c;
//             ++numRecognizedChannels;
//         }
//         else if (channelName == "b")
//         {
//             idxB = c;
//             ++numRecognizedChannels;
//         }
//         else if (channelName == "a")
//         {
//             ++numRecognizedChannels;
//         }
//     }

//     auto rawImgChn = reinterpret_cast<float**>(exrImage.images);
//     bool loaded = false;

//     hdrPixels = new float[imgWidth * imgHeight * 3];

    
//     if (numRecognizedChannels == 1)             // 1 channel images can be loaded into either scalar or vector formats.
//     {
//         for (int y = 0; y < imgHeight; y++)
//         {
//             for (int x = 0; x < imgWidth; x++)
//             {
//                 int linearIdx = y * imgWidth + x;
//                 float color(rawImgChn[0][linearIdx]);
//                 hdrPixels[3 * linearIdx + 0] = color;
//                 hdrPixels[3 * linearIdx + 1] = color;
//                 hdrPixels[3 * linearIdx + 2] = color;
//             }
//         }
//         loaded = true;
//     }        
//     else if (numRecognizedChannels == 2)        // 2 channel images can only be loaded into vector2/3/4 formats.
//     {
//         assert(idxR != -1 && idxG != -1);
//         for (int y = 0; y < imgHeight; y++)
//         {
//             for (int x = 0; x < imgWidth; x++)
//             {
//                 int linearIdx = y * imgWidth + x;
//                 hdrPixels[3 * linearIdx + 0] = rawImgChn[idxR][linearIdx];
//                 hdrPixels[3 * linearIdx + 1] = rawImgChn[idxG][linearIdx];
//                 hdrPixels[3 * linearIdx + 2] = 0.0f;
//             }
//         }
//         loaded = true;
//     }
//     else if (numRecognizedChannels == 3 || numRecognizedChannels == 4) // 3 or 4 channel images can only be loaded into vector3/4 formats.
//     {
//         assert(idxR != -1 && idxG != -1 && idxB != -1);
//         for (int y = 0; y < imgHeight; y++)
//         {
//             for (int x = 0; x < imgWidth; x++)
//             {
//                 int linearIdx = y * imgWidth + x;
//                 hdrPixels[3 * linearIdx + 0] = rawImgChn[idxR][linearIdx];
//                 hdrPixels[3 * linearIdx + 1] = rawImgChn[idxG][linearIdx];
//                 hdrPixels[3 * linearIdx + 2] = rawImgChn[idxB][linearIdx];
//             }
//         }
//         loaded = true;
//     }

//     FreeEXRHeader(&exrHeader);
//     FreeEXRImage(&exrImage);

//     if (!loaded)
//     {
//         std::cerr << "Insufficient target channels when loading EXR: need " << exrHeader.num_channels << std::endl;
//         return false;
//     }
//     else
//     {
//         return true;
//     }
// }

// // Note that when an image us loaded, the variable pixels is allocated, and it is up to the user to deallocate it later.
// bool loadImage(const std::string& fileName, int& imgWidth, int& imgHeight, float*& pixels)
// {
//     bool bOk = false;
//     std::string extension = fileName.substr(fileName.find_last_of(".") + 1);
//     if (extension == "png" || extension == "bmp" || extension == "tga")
//     {
//         bOk = ldrLoad(fileName, imgWidth, imgHeight, pixels);
//     }
//     else if (extension == "exr")
//     {
//         bOk = hdrLoad(fileName, imgWidth, imgHeight, pixels);
//     }

//     return bOk;
// }

// bool load(FLIP::image<FLIP::color3>& dstImage, const std::string& fileName)
// {
//     int imgWidth;
//     int imgHeight;
//     float* pixels;
//     if (loadImage(fileName, imgWidth, imgHeight, pixels))
//     {
//         dstImage.setPixels(pixels, imgWidth, imgHeight);
//     }
//     return false;
// }

OIIO_NAMESPACE_END