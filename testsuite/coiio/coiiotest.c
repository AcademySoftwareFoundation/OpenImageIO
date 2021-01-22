// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#include <OpenImageIO/c-imageio.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    // storage we'll use for error messages
    char errmsg[256];

    // open the test image
    const char* infile  = "src/checker.tif";
    OIIO_ImageInput* ii = OIIO_ImageInput_open(infile, NULL, NULL);
    if (!ii) {
        fprintf(stderr, "Could not open file \"%s\"\n", infile);
        if (OIIO_haserror()) {
            OIIO_geterror(errmsg, sizeof(errmsg), true);
            fprintf(stderr, "    %s\n", errmsg);
        }
        return -1;
    }

    // Get the image dimensions
    const OIIO_ImageSpec* in_spec = OIIO_ImageInput_spec(ii);
    int w                         = OIIO_ImageSpec_width(in_spec);
    int h                         = OIIO_ImageSpec_height(in_spec);
    printf("Dimensions are %dx%d\n", w, h);

    printf("Channels are:\n");
    int nchannels = OIIO_ImageSpec_nchannels(in_spec);
    for (int i = 0; i < nchannels; ++i) {
        printf("    %s\n", OIIO_ImageSpec_channel_name(in_spec, i));
    }

    // read image data
    float* data = (float*)malloc(sizeof(float) * w * h * nchannels);
    bool result = OIIO_ImageInput_read_image(ii,
                                             0,                // subimage
                                             0,                // miplevel
                                             0,                // chbegin
                                             nchannels,        // chend
                                             OIIO_TypeFloat,   //format
                                             data,             // pixel storage
                                             OIIO_AutoStride,  // xstride
                                             OIIO_AutoStride,  // ystride
                                             OIIO_AutoStride,  // zstride
                                             NULL,  // progress_callback
                                             NULL   // progress_callback_data
    );

    if (!result) {
        fprintf(stderr, "Error loading \"%s\" because:\n", infile);
        if (OIIO_ImageInput_has_error(ii)) {
            OIIO_ImageInput_geterror(ii, errmsg, sizeof(errmsg), true);
            fprintf(stderr, "    %s\n", errmsg);
        } else {
            fprintf(stderr, "    unknown: no errors on ImageInput\n");
        }
    }

    // create a new image spec for our output image by copying the input one
    OIIO_ImageSpec* out_spec = OIIO_ImageSpec_copy(in_spec);

    // set a couple of test attributes
    int test_int_attr = 17;
    OIIO_ImageSpec_attribute(out_spec, "test_int_attr", OIIO_TypeInt,
                             &test_int_attr);

    const char* test_str_attr = "the quick brown fox...";
    OIIO_ImageSpec_attribute(out_spec, "test_str_attr", OIIO_TypeString,
                             &test_str_attr);

    // create the output image
    OIIO_ImageOutput* io = OIIO_ImageOutput_create("out.exr", NULL, "");
    if (!io) {
        fprintf(stderr, "could not open out.exr\n");
        if (OIIO_haserror()) {
            OIIO_geterror(errmsg, sizeof(errmsg), true);
            fprintf(stderr, "    %s\n", errmsg);
        }
        return -2;
    }

    result = OIIO_ImageOutput_open(io, "out.exr", out_spec,
                                   OIIO_ImageOutput_OpenMode_Create);
    if (!result) {
        fprintf(stderr, "Error opening \"out.exr\" because:\n");
        if (OIIO_ImageOutput_has_error(io)) {
            OIIO_ImageOutput_geterror(io, errmsg, sizeof(errmsg), true);
            fprintf(stderr, "    %s\n", errmsg);
        } else {
            fprintf(stderr, "    unknown: no errors on ImageOutput\n");
        }
    }

    // write the image
    result = OIIO_ImageOutput_write_image(io,
                                          OIIO_TypeFloat,   // format
                                          data,             // data
                                          OIIO_AutoStride,  // xstride
                                          OIIO_AutoStride,  // ystride
                                          OIIO_AutoStride,  // zstride
                                          NULL,             // progress_callback
                                          NULL  // progress_callback_data
    );

    if (!result) {
        fprintf(stderr, "Error writing \"out.exr\" because:\n");
        if (OIIO_ImageOutput_has_error(io)) {
            OIIO_ImageOutput_geterror(io, errmsg, sizeof(errmsg), true);
            fprintf(stderr, "    %s\n", errmsg);
        } else {
            fprintf(stderr, "    unknown: no errors on ImageOutput\n");
        }
    }


    OIIO_ImageInput_delete(ii);
    OIIO_ImageOutput_delete(io);
    free(data);

    // re-open the output image and read the metadata to check our attributes
    // are in there
    ii = OIIO_ImageInput_open("out.exr", NULL, NULL);
    if (!ii) {
        fprintf(stderr, "Could not open file \"out.exr\"\n");
        if (OIIO_haserror()) {
            OIIO_geterror(errmsg, sizeof(errmsg), true);
            fprintf(stderr, "    %s\n", errmsg);
        }
        return -1;
    }

    // Get the image dimensions
    in_spec = OIIO_ImageInput_spec(ii);

    int o_int_attr = 0;
    if (OIIO_ImageSpec_getattribute(in_spec, "test_int_attr", OIIO_TypeInt,
                                    &o_int_attr, true)) {
        printf("test_int_attr: %d\n", o_int_attr);
    } else {
        fprintf(stderr, "Could not get test_int_attr\n");
    }

    char* o_str_attr;
    if (OIIO_ImageSpec_getattribute(in_spec, "test_str_attr", OIIO_TypeString,
                                    &o_str_attr, true)) {
        printf("test_str_attr: %s\n", o_str_attr);
    } else {
        fprintf(stderr, "Could not get test_str_attr\n");
    }

    OIIO_ImageInput_delete(ii);

    return 0;
}
