/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/

// Based on the sample at:
// http://code.google.com/p/googletest/wiki/GoogleTestPrimer#Writing_the_main()_Function

#include "imageio.h"
#include "imagebuf.h"
#include "imagebufalgo.h"
#include "sysutil.h"
#include "unittest.h"

#include <iostream>
#include <iomanip>
#include <string>

OIIO_NAMESPACE_USING;


// Test ImageBuf::zero and ImageBuf::fill
void ImageBuf_zero_fill ()
{
    const int WIDTH = 8;
    const int HEIGHT = 6;
    const int CHANNELS = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;

    // Create a buffer -- pixels should be undefined
    ImageBuf A ("A", spec);
    
    // Set a pixel to an odd value, make sure it takes
    const float arbitrary1[CHANNELS] = { 0.2, 0.3, 0.4, 0.5 };
    A.setpixel (1, 1, arbitrary1);
    float pixel[CHANNELS];   // test pixel
    A.getpixel (1, 1, pixel);
    for (int c = 0;  c < CHANNELS;  ++c)
        OIIO_CHECK_EQUAL (pixel[c], arbitrary1[c]);

    // Zero out and test that it worked
    ImageBufAlgo::zero (A);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            A.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], 0.0f);
        }
    }

    // Test fill of whole image
    const float arbitrary2[CHANNELS] = { 0.6, 0.7, 0.3, 0.9 };
    ImageBufAlgo::fill (A, arbitrary2);
    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            A.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], arbitrary2[c]);
        }
    }

    // Test fill of partial image
    const float arbitrary3[CHANNELS] = { 0.42, 0.43, 0.44, 0.45 };
    {
        const int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
        ImageBufAlgo::fill (A, arbitrary3, xbegin, xend, ybegin, yend);
        for (int j = 0;  j < HEIGHT;  ++j) {
            for (int i = 0;  i < WIDTH;  ++i) {
                float pixel[CHANNELS];
                A.getpixel (i, j, pixel);
                if (j >= ybegin && j < yend && i >= xbegin && i < xend) {
                    for (int c = 0;  c < CHANNELS;  ++c)
                        OIIO_CHECK_EQUAL (pixel[c], arbitrary3[c]);
                } else {
                    for (int c = 0;  c < CHANNELS;  ++c)
                        OIIO_CHECK_EQUAL (pixel[c], arbitrary2[c]);
                }
            }
        }
    }
}



// Test ImageBuf::crop
void test_crop ()
{
    int WIDTH = 8, HEIGHT = 6, CHANNELS = 4;
    // Crop region we'll work with
    int xbegin = 3, xend = 5, ybegin = 0, yend = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;
    ImageBuf A, B;
    A.reset ("A", spec);
    B.reset ("B", spec);
    float arbitrary1[4];
    arbitrary1[0] = 0.2;
    arbitrary1[1] = 0.3;
    arbitrary1[2] = 0.4;
    arbitrary1[3] = 0.5;
    ImageBufAlgo::fill (A, arbitrary1);

    // Test CUT crop
    ImageBufAlgo::crop (B, A, xbegin, xend, ybegin, yend);

    // Should have changed the data window (origin and width/height)
    OIIO_CHECK_EQUAL (B.spec().x, xbegin);
    OIIO_CHECK_EQUAL (B.spec().width, xend-xbegin);
    OIIO_CHECK_EQUAL (B.spec().y, ybegin);
    OIIO_CHECK_EQUAL (B.spec().height, yend-ybegin);
    float *pixel = ALLOCA(float, CHANNELS);
    for (int j = 0;  j < B.spec().height;  ++j) {
        for (int i = 0;  i < B.spec().width;  ++i) {
            B.getpixel (i+B.xbegin(), j+B.ybegin(), pixel);
            // Inside the crop region should match what it always was
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], arbitrary1[c]);
        }
    }
}



// Tests ImageBufAlgo::add
void ImageBuf_add ()
{
    const int WIDTH = 8;
    const int HEIGHT = 8;
    const int CHANNELS = 4;
    ImageSpec spec (WIDTH, HEIGHT, CHANNELS, TypeDesc::FLOAT);
    spec.alpha_channel = 3;

    // Create buffers
    ImageBuf A ("A", spec);
    const float Aval[CHANNELS] = { 0.1, 0.2, 0.3, 0.4 };
    ImageBufAlgo::fill (A, Aval);
    ImageBuf B ("B", spec);
    const float Bval[CHANNELS] = { 0.01, 0.02, 0.03, 0.04 };
    ImageBufAlgo::fill (B, Bval);

    ImageBuf C ("C", spec);
    ImageBufAlgo::add (C, A, B);

    for (int j = 0;  j < HEIGHT;  ++j) {
        for (int i = 0;  i < WIDTH;  ++i) {
            float pixel[CHANNELS];
            C.getpixel (i, j, pixel);
            for (int c = 0;  c < CHANNELS;  ++c)
                OIIO_CHECK_EQUAL (pixel[c], Aval[c]+Bval[c]);
        }
    }
}



// check if two float values match
inline bool
is_equal (float x, float y) {
    return std::abs(x - y) <= 1e-05 * std::abs(x); // 1e-05
}



// luminance ramp from -0.1 -> 14.0 used in color transfer tests
static float LUMINANCE_LINEAR[142] = {
-0.100000,  0.000000,  0.100000,  0.200000,  0.300000,  0.400000,  0.500000, 
 0.600000,  0.700000,  0.800000,  0.900000,  1.000000,  1.100000,  1.200000, 
 1.300000,  1.400000,  1.500000,  1.600000,  1.700000,  1.800000,  1.900000, 
 2.000000,  2.100000,  2.200000,  2.300000,  2.400000,  2.500000,  2.600000, 
 2.700000,  2.800000,  2.900000,  3.000000,  3.100000,  3.200000,  3.300000, 
 3.400000,  3.500000,  3.600000,  3.700000,  3.800000,  3.900000,  4.000000, 
 4.100000,  4.200000,  4.300000,  4.400000,  4.500000,  4.600000,  4.700000, 
 4.800000,  4.900000,  5.000000,  5.100000,  5.200000,  5.300000,  5.400000, 
 5.500000,  5.600000,  5.700000,  5.800000,  5.900000,  6.000000,  6.100000, 
 6.200000,  6.300000,  6.400000,  6.500000,  6.600000,  6.700000,  6.800000, 
 6.900000,  7.000000,  7.100000,  7.200000,  7.300000,  7.400000,  7.500000, 
 7.600000,  7.700000,  7.800000,  7.900000,  8.000000,  8.100000,  8.200000, 
 8.300000,  8.400000,  8.500000,  8.600000,  8.700000,  8.800000,  8.900000, 
 9.000000,  9.100000,  9.200000,  9.300000,  9.400000,  9.500000,  9.600000, 
 9.700000,  9.800000,  9.900000, 10.000000, 10.100000, 10.200000, 10.300000, 
10.400000, 10.500000, 10.600000, 10.700000, 10.800000, 10.900000, 11.000000, 
11.100000, 11.200000, 11.300000, 11.400000, 11.500000, 11.600000, 11.700000, 
11.800000, 11.900000, 12.000000, 12.100000, 12.200000, 12.300000, 12.400000, 
12.500000, 12.600000, 12.700000, 12.800000, 12.900000, 13.000000, 13.100000, 
13.200000, 13.200000, 13.400000, 13.500000, 13.600000, 13.700000, 13.800000, 
13.900000, 14.000000
};

// The fixture for testing class ImageBufAlgo::ColorTransfer.
class ColorTransferTest {
    
public:
    
    // You can do set-up work for each test here.
    ColorTransferTest () : tfunc(NULL) {
        CHANNELS = 4;
        ALPHA = 1.f;
        STEPS = sizeof(LUMINANCE_LINEAR) / sizeof(float);
        ImageSpec spec (1, STEPS, CHANNELS, TypeDesc::FLOAT);
        spec.alpha_channel = 3;
        input.reset ("input", spec);
        output.reset ("output", spec);
        reverse.reset ("reverse", spec);
        PRINT_OUTPUT = false;
        PRINT_REVERSE = false;

        // Create an input ramp from the LUMINANCE_LINEAR array
        for(int y = 0; y < STEPS; y++) {
            float pixel[4] = {LUMINANCE_LINEAR[y], LUMINANCE_LINEAR[y],
                              LUMINANCE_LINEAR[y], ALPHA};
            input.setpixel (0, y, pixel);
        }
    }
    
    ~ColorTransferTest () {
        // Print the output ramp for when you need to change the
        // LUMINANCE_<COLORSPACE> array values
        if(PRINT_OUTPUT) {
            ImageBuf::ConstIterator<float,float> output_itr (output);
            int c = 0;
            for ( ; output_itr.valid(); ++output_itr) {
                std::cerr << std::right << std::fixed << std::setw(9) << output_itr[0] << ", ";
                if(c > 5) { std::cerr << std::endl; c = -1; }
                c++;
            }
            std::cerr << std::endl;
        }
        
        // Print the reverse ramp
        if(PRINT_REVERSE) {
            ImageBuf::ConstIterator<float,float> reverse_itr (reverse);
            int c = 0;
            for ( ; reverse_itr.valid(); ++reverse_itr) {
                std::cerr << std::right << std::fixed << std::setw(9) << reverse_itr[0] << ", ";
                if(c > 5) { std::cerr << std::endl; c = -1; }
                c++;
            }
            std::cerr << std::endl;
        }

        delete tfunc;
    }
    
    void
    check_transfer(const ImageBuf &in, const float *table, int size) {
        // skip the first pixel which will be an invalid negative luminance
        // value, this value is just to test the fwd transfer doesn't create
        // nan's.
        float *pixel = ALLOCA(float, CHANNELS);
        for (int y = 1; y < STEPS; y++) {
            in.getpixel (0, y, pixel);
            for (int c = 0; c < CHANNELS; ++c) {
                if (c == in.spec().alpha_channel ||
                    c == in.spec().z_channel)
                    OIIO_CHECK_EQUAL (pixel[c], ALPHA);
                else
                    OIIO_CHECK_ASSERT (is_equal(pixel[c], table[y]));
            }
        }
    }
    
    // check if two float values match
    inline bool
    is_equal (float x, float y) {
        return std::abs(x - y) <= 1e-05 * std::abs(x); // 1e-05
    }
    
    // Objects declared here can be used by all tests in the test case for Foo.
    int CHANNELS, STEPS;
    float ALPHA;
    ImageSpec spec;
    ImageBuf input, output, reverse;
    ColorTransfer *tfunc;
    bool PRINT_OUTPUT, PRINT_REVERSE;
    
};


void ColourTransferLinear ()
{
    ColorTransferTest t;

    // uncomment this if you want to print the output luminance table
    //t.PRINT_OUTPUT = true;
    //t.PRINT_REVERSE = true;
    
    // create a color transfer function and use it
    t.tfunc = ColorTransfer::create ("null");
    ImageBufAlgo::colortransfer (t.output, t.input, t.tfunc);
    
    // check the transfer was done correctly
    t.check_transfer (t.output, LUMINANCE_LINEAR,
                      sizeof(LUMINANCE_LINEAR) / sizeof(float));
}


// Gamma 2.2 luminance ramp
static float LUMINANCE_Gamma22[142] = {
-0.100000,  0.000000,  0.351119,  0.481157,  0.578533,  0.659353,  0.729740, 
0.792793,  0.850335,  0.903545,  0.953238,  1.000000,  1.044275,  1.086404, 
1.126659,  1.165257,  1.202379,  1.238174,  1.272769,  1.306270,  1.338771, 
1.370351,  1.401081,  1.431023,  1.460232,  1.488755,  1.516638,  1.543918, 
1.570632,  1.596811,  1.622486,  1.647682,  1.672423,  1.696733,  1.720633, 
1.744140,  1.767273,  1.790048,  1.812481,  1.834586,  1.856375,  1.877862, 
1.899057,  1.919973,  1.940619,  1.961004,  1.981138,  2.001030,  2.020687, 
2.040117,  2.059328,  2.078326,  2.097118,  2.115710,  2.134108,  2.152317, 
2.170344,  2.188192,  2.205868,  2.223375,  2.240718,  2.257902,  2.274930, 
2.291807,  2.308536,  2.325120,  2.341564,  2.357871,  2.374043,  2.390084, 
2.405997,  2.421784,  2.437449,  2.452995,  2.468422,  2.483735,  2.498936, 
2.514026,  2.529009,  2.543885,  2.558658,  2.573330,  2.587901,  2.602375, 
2.616753,  2.631037,  2.645228,  2.659329,  2.673340,  2.687264,  2.701101, 
2.714855,  2.728525,  2.742113,  2.755621,  2.769050,  2.782402,  2.795676, 
2.808876,  2.822002,  2.835055,  2.848036,  2.860946,  2.873787,  2.886560, 
2.899265,  2.911903,  2.924476,  2.936985,  2.949430,  2.961812,  2.974132, 
2.986392,  2.998591,  3.010731,  3.022813,  3.034837,  3.046804,  3.058715, 
3.070570,  3.082371,  3.094118,  3.105812,  3.117453,  3.129042,  3.140580, 
3.152067,  3.163504,  3.174892,  3.186231,  3.197521,  3.208765,  3.219961, 
3.231110,  3.231110,  3.253272,  3.264285,  3.275254,  3.286178,  3.297060, 
3.307898,  3.318694
};


void ColourTransferGamma22 ()
{
    ColorTransferTest t;

    // uncomment this if you want to print the output luminance table
    //t.PRINT_OUTPUT = true;
    //t.PRINT_REVERSE = true;
    
    // create a color transfer function and use it
    t.tfunc = ColorTransfer::create ("gamma");
    (*t.tfunc).set("gamma", 1/2.2);
    ImageBufAlgo::colortransfer (t.output, t.input, t.tfunc);
    
    // check the transfer was done correctly
    t.check_transfer (t.output, LUMINANCE_Gamma22,
                    sizeof(LUMINANCE_Gamma22) / sizeof(float));
    
    // do the reverse transfer
    (*t.tfunc).set("gamma", 2.2);
    ImageBufAlgo::colortransfer (t.reverse, t.output, t.tfunc);
    
    // check reverse transfer with LUMINANCE_LINEAR values
    t.check_transfer (t.reverse, LUMINANCE_LINEAR,
                      sizeof(LUMINANCE_LINEAR) / sizeof(float));
}



// sRGB luminance ramp
static float LUMINANCE_SRGB[142] = {
0.000000,  0.000000,  0.349190,  0.484529,  0.583831,  0.665185,  0.735357, 
0.797738,  0.854306,  0.906332,  0.954687,  1.000000,  1.042740,  1.083268, 
1.121871,  1.158778,  1.194176,  1.228224,  1.261051,  1.292771,  1.323478, 
1.353256,  1.382178,  1.410307,  1.437699,  1.464406,  1.490471,  1.515934, 
1.540833,  1.565199,  1.589062,  1.612451,  1.635388,  1.657898,  1.680002, 
1.701718,  1.723064,  1.744058,  1.764714,  1.785047,  1.805070,  1.824796, 
1.844236,  1.863402,  1.882303,  1.900950,  1.919351,  1.937515,  1.955450, 
1.973163,  1.990663,  2.007956,  2.025048,  2.041945,  2.058655,  2.075181, 
2.091530,  2.107706,  2.123714,  2.139560,  2.155247,  2.170779,  2.186162, 
2.201397,  2.216491,  2.231445,  2.246264,  2.260949,  2.275506,  2.289937, 
2.304244,  2.318431,  2.332500,  2.346454,  2.360296,  2.374027,  2.387650, 
2.401168,  2.414582,  2.427896,  2.441110,  2.454227,  2.467248,  2.480176, 
2.493013,  2.505759,  2.518418,  2.530990,  2.543476,  2.555880,  2.568201, 
2.580442,  2.592604,  2.604688,  2.616696,  2.628628,  2.640487,  2.652273, 
2.663988,  2.675633,  2.687208,  2.698716,  2.710156,  2.721531,  2.732841, 
2.744087,  2.755269,  2.766391,  2.777450,  2.788450,  2.799391,  2.810273, 
2.821098,  2.831866,  2.842578,  2.853235,  2.863837,  2.874386,  2.884882, 
2.895325,  2.905718,  2.916059,  2.926350,  2.936592,  2.946785,  2.956929, 
2.967026,  2.977076,  2.987080,  2.997038,  3.006950,  3.016818,  3.026642, 
3.036422,  3.036422,  3.055852,  3.065505,  3.075115,  3.084684,  3.094213, 
3.103701,  3.113150
};


void ColourTransferSRGB ()
{
    ColorTransferTest t;

    // uncomment this if you want to print the output luminance table
    //t.PRINT_OUTPUT = true;
    //t.PRINT_REVERSE = true;
    
    // create a color transfer function and use it
    t.tfunc = ColorTransfer::create ("linear_to_sRGB");
    ImageBufAlgo::colortransfer (t.output, t.input, t.tfunc);

    // check the transfer was done correctly
    t.check_transfer (t.output, LUMINANCE_SRGB,
                      sizeof(LUMINANCE_SRGB) / sizeof(float));
    
    // do the reverse transfer
    t.tfunc = ColorTransfer::create ("sRGB_to_linear");
    ImageBufAlgo::colortransfer (t.reverse, t.output, t.tfunc);
    
    // check reverse transfer with LUMINANCE_LINEAR values
    t.check_transfer (t.reverse, LUMINANCE_LINEAR,
                      sizeof(LUMINANCE_LINEAR) / sizeof(float));
}



// AdobeRGB luminance ramp
static float LUMINANCE_AdobeRGB[142] = {
0.000000,  0.000000,  0.350989,  0.481031,  0.578420,  0.659256,  0.729658, 
0.792727,  0.850286,  0.903513,  0.953221,  1.000000,  1.044291,  1.086436, 
1.126707,  1.165321,  1.202458,  1.238268,  1.272878,  1.306394,  1.338909, 
1.370504,  1.401249,  1.431205,  1.460428,  1.488966,  1.516862,  1.544156, 
1.570884,  1.597077,  1.622765,  1.647974,  1.672729,  1.697052,  1.720964, 
1.744485,  1.767631,  1.790419,  1.812864,  1.834981,  1.856783,  1.878282, 
1.899490,  1.920418,  1.941076,  1.961473,  1.981619,  2.001523,  2.021192, 
2.040634,  2.059856,  2.078866,  2.097670,  2.116273,  2.134682,  2.152903, 
2.170941,  2.188801,  2.206488,  2.224006,  2.241361,  2.258555,  2.275595, 
2.292482,  2.309222,  2.325818,  2.342272,  2.358589,  2.374772,  2.390824, 
2.406747,  2.422545,  2.438221,  2.453777,  2.469215,  2.484538,  2.499749, 
2.514850,  2.529842,  2.544729,  2.559513,  2.574194,  2.588776,  2.603260, 
2.617648,  2.631941,  2.646142,  2.660253,  2.674274,  2.688208,  2.702055, 
2.715818,  2.729498,  2.743096,  2.756614,  2.770052,  2.783413,  2.796698, 
2.809907,  2.823042,  2.836104,  2.849095,  2.862015,  2.874865,  2.887647, 
2.900361,  2.913009,  2.925591,  2.938109,  2.950563,  2.962955,  2.975284, 
2.987553,  2.999761,  3.011911,  3.024001,  3.036034,  3.048010,  3.059930, 
3.071795,  3.083604,  3.095360,  3.107063,  3.118713,  3.130311,  3.141857, 
3.153353,  3.164799,  3.176195,  3.187543,  3.198842,  3.210094,  3.221299, 
3.232457,  3.232457,  3.254636,  3.265657,  3.276635,  3.287568,  3.298458, 
3.309305,  3.320109
};



void ColourTransferAdobeRGB ()
{
    ColorTransferTest t;

    // uncomment this if you want to print the output luminance table
    //t.PRINT_OUTPUT = true;
    //t.PRINT_REVERSE = true;
    
    // create a color transfer function and use it
    t.tfunc = ColorTransfer::create ("linear_to_AdobeRGB");
    ImageBufAlgo::colortransfer (t.output, t.input, t.tfunc);

    // check the transfer was done correctly
    t.check_transfer (t.output, LUMINANCE_AdobeRGB,
                      sizeof(LUMINANCE_AdobeRGB) / sizeof(float));
    
    // do the reverse transfer
    t.tfunc = ColorTransfer::create ("AdobeRGB_to_linear");
    ImageBufAlgo::colortransfer (t.reverse, t.output, t.tfunc);
    
    // check reverse transfer with LUMINANCE_LINEAR values
    t.check_transfer (t.reverse, LUMINANCE_LINEAR,
                      sizeof(LUMINANCE_LINEAR) / sizeof(float));
}



// Rec709 luminance ramp
static float LUMINANCE_Rec709[142] = {
0.000000,  0.000000,  0.290940,  0.433674,  0.540296,  0.628654,  0.705515, 
0.774305,  0.837034,  0.895004,  0.949110,  1.000000,  1.048161,  1.093969, 
1.137722,  1.179661,  1.219982,  1.258850,  1.296403,  1.332760,  1.368023, 
1.402278,  1.435604,  1.468068,  1.499730,  1.530644,  1.560857,  1.590412, 
1.619349,  1.647702,  1.675503,  1.702782,  1.729565,  1.755877,  1.781741, 
1.807177,  1.832205,  1.856842,  1.881106,  1.905012,  1.928574,  1.951807, 
1.974721,  1.997331,  2.019646,  2.041678,  2.063436,  2.084930,  2.106168, 
2.127159,  2.147911,  2.168432,  2.188727,  2.208805,  2.228672,  2.248334, 
2.267796,  2.287065,  2.306145,  2.325043,  2.343761,  2.362307,  2.380682, 
2.398894,  2.416944,  2.434837,  2.452577,  2.470168,  2.487612,  2.504914, 
2.522077,  2.539104,  2.555996,  2.572759,  2.589394,  2.605905,  2.622293, 
2.638561,  2.654712,  2.670748,  2.686671,  2.702484,  2.718189,  2.733787, 
2.749281,  2.764673,  2.779964,  2.795156,  2.810252,  2.825253,  2.840160, 
2.854975,  2.869700,  2.884336,  2.898885,  2.913348,  2.927727,  2.942023, 
2.956237,  2.970371,  2.984426,  2.998403,  3.012303,  3.026128,  3.039878, 
3.053555,  3.067160,  3.080694,  3.094158,  3.107553,  3.120879,  3.134139, 
3.147333,  3.160461,  3.173525,  3.186526,  3.199464,  3.212340,  3.225155, 
3.237911,  3.250607,  3.263244,  3.275824,  3.288346,  3.300813,  3.313223, 
3.325579,  3.337880,  3.350128,  3.362323,  3.374466,  3.386557,  3.398597, 
3.410586,  3.410586,  3.434416,  3.446258,  3.458052,  3.469798,  3.481497, 
3.493149,  3.504755
};

void ColourTransferRec709 ()
{
    ColorTransferTest t;

    // uncomment this if you want to print the output luminance table
    //PRINT_OUTPUT = true;
    //PRINT_REVERSE = true;
    
    // create a color transfer function and use it
    t.tfunc = ColorTransfer::create ("linear_to_Rec709");
    ImageBufAlgo::colortransfer (t.output, t.input, t.tfunc);
    
    // check the transfer was done correctly
    t.check_transfer (t.output, LUMINANCE_Rec709,
                      sizeof(LUMINANCE_Rec709) / sizeof(float));
    
    // do the reverse transfer
    t.tfunc = ColorTransfer::create ("Rec709_to_linear");
    ImageBufAlgo::colortransfer (t.reverse, t.output, t.tfunc);
    
    // check reverse transfer with LUMINANCE_LINEAR values
    t.check_transfer (t.reverse, LUMINANCE_LINEAR,
                      sizeof(LUMINANCE_LINEAR) / sizeof(float));
}



// KodakLog luminance ramp
static float LUMINANCE_KodakLog[142] = {
0.092864,  0.092864,  0.388156,  0.470008,  0.519431,  0.554948,  0.582688, 
0.605454,  0.624761,  0.641523,  0.656333,  0.669599,  0.681613,  0.692590, 
0.702696,  0.712058,  0.720780,  0.728942,  0.736612,  0.743847,  0.750692, 
0.757188,  0.763369,  0.769264,  0.774898,  0.780293,  0.785469,  0.790443, 
0.795230,  0.799843,  0.804296,  0.808597,  0.812759,  0.816788,  0.820694, 
0.824484,  0.828164,  0.831741,  0.835220,  0.838606,  0.841905,  0.845121, 
0.848257,  0.851318,  0.854307,  0.857228,  0.860083,  0.862876,  0.865608, 
0.868283,  0.870903,  0.873471,  0.875987,  0.878455,  0.880876,  0.883252, 
0.885584,  0.887875,  0.890124,  0.892335,  0.894508,  0.896645,  0.898746, 
0.900814,  0.902848,  0.904850,  0.906821,  0.908762,  0.910675,  0.912558, 
0.914415,  0.916244,  0.918048,  0.919827,  0.921581,  0.923311,  0.925018, 
0.926702,  0.928365,  0.930006,  0.931626,  0.933226,  0.934806,  0.936367, 
0.937908,  0.939432,  0.940937,  0.942425,  0.943895,  0.945349,  0.946786, 
0.948207,  0.949613,  0.951003,  0.952379,  0.953739,  0.955085,  0.956417, 
0.957736,  0.959040,  0.960332,  0.961611,  0.962876,  0.964130,  0.965371, 
0.966600,  0.967818,  0.969024,  0.970218,  0.971402,  0.972575,  0.973737, 
0.974888,  0.976029,  0.977160,  0.978281,  0.979392,  0.980494,  0.981586, 
0.982669,  0.983743,  0.984808,  0.985864,  0.986911,  0.987950,  0.988980, 
0.990002,  0.991016,  0.992022,  0.993020,  0.994010,  0.994993,  0.995968, 
0.996936,  0.996936,  0.998850,  0.999796,  1.000735,  1.001667,  1.002593, 
1.003511,  1.004424
};

void ColourTransferKodakLog ()
{
    ColorTransferTest t;

    // uncomment this if you want to print the output luminance table
    //t.PRINT_OUTPUT = true;
    //t.PRINT_REVERSE = true;
    
    // create a color transfer function and use it
    t.tfunc = ColorTransfer::create ("linear_to_KodakLog");
    (*t.tfunc).set("refBlack", 95.f);
    (*t.tfunc).set("refWhite", 685.f);
    (*t.tfunc).set("dispGamma", 1.7f);
    (*t.tfunc).set("negGamma", 0.6f);
    ImageBufAlgo::colortransfer (t.output, t.input, t.tfunc);

    // check the transfer was done correctly
    t.check_transfer (t.output, LUMINANCE_KodakLog,
                      sizeof(LUMINANCE_KodakLog) / sizeof(float));
    
    // do the reverse transfer
    t.tfunc = ColorTransfer::create ("KodakLog_to_linear");
    (*t.tfunc).set("refBlack", 95.f);
    (*t.tfunc).set("refWhite", 685.f);
    (*t.tfunc).set("dispGamma", 1.7f);
    (*t.tfunc).set("negGamma", 0.6f);
    ImageBufAlgo::colortransfer (t.reverse, t.output, t.tfunc);
    
    // check reverse transfer with LUMINANCE_LINEAR values
    t.check_transfer (t.reverse, LUMINANCE_LINEAR,
                      sizeof(LUMINANCE_LINEAR) / sizeof(float));
}



int
main (int argc, char **argv)
{
    ImageBuf_zero_fill ();
    test_crop ();
    ImageBuf_add ();
    ColourTransferLinear ();
    ColourTransferGamma22 ();
    ColourTransferSRGB ();
    ColourTransferAdobeRGB ();
    ColourTransferRec709 ();
    ColourTransferKodakLog ();

    return unit_test_failures;
}
