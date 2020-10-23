#include <OpenImageIO-C/c-imageio.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    // storage we'll use for error messages
    const int errmsglen = 256;
    char errmsg[errmsglen];

    // open the test image
    const char* infile = "data/checker.tif";
    ImageInput* ii     = ImageInput_open(infile, NULL, NULL);
    if (!ii) {
        fprintf(stderr, "Could not open file \"%s\"\n", infile);
        if (openimageio_haserror()) {
            openimageio_geterror(errmsg, errmsglen, true);
            fprintf(stderr, "    %s\n", errmsg);
        }
        return -1;
    }

    // Get the image dimensions
    const ImageSpec* in_spec = ImageInput_spec(ii);
    int w                    = ImageSpec_width(in_spec);
    int h                    = ImageSpec_height(in_spec);
    printf("Dimensions are %dx%d\n", w, h);

    printf("Channels are:\n");
    int nchannels = ImageSpec_nchannels(in_spec);
    for (int i = 0; i < nchannels; ++i) {
        printf("    %s\n", ImageSpec_channel_name(in_spec, i));
    }

    // read image data
    float* data = (float*)malloc(sizeof(float) * w * h * nchannels);
    bool result = ImageInput_read_image(ii,
                                        0,          // subimage
                                        0,          // miplevel
                                        0,          // chbegin
                                        nchannels,  // chend
                                        OIIO_TypeFloat,  //format
                                        data,        // pixel storage
                                        OIIO_AutoStride,  // xstride
                                        OIIO_AutoStride,  // ystride
                                        OIIO_AutoStride,  // zstride
                                        NULL,     // progress_callback
                                        NULL      // progress_callback_data
    );

    if (!result) {
        fprintf(stderr, "Error loading \"%s\" because:\n", infile);
        if (ImageInput_has_error(ii)) {
            ImageInput_geterror(ii, errmsg, errmsglen, true);
            fprintf(stderr, "    %s\n", errmsg);
        } else {
            fprintf(stderr, "    unknown: no errors on ImageInput\n");
        }
    }

    // create a new image spec for our output image by copying the input one
    ImageSpec* out_spec = ImageSpec_copy(in_spec);

    // set a couple of test attributes
    int test_int_attr = 17;
    ImageSpec_attribute(out_spec, "test_int_attr", OIIO_TypeInt,
                        &test_int_attr);

    const char* test_str_attr = "the quick brown fox...";
    ImageSpec_attribute(out_spec, "test_str_attr",
                        OIIO_TypeString, &test_str_attr);

    // create the output image
    ImageOutput* io = ImageOutput_create("out.exr", NULL, "");
    if (!io) {
        fprintf(stderr, "could not open out.exr\n");
        if (openimageio_haserror()) {
            openimageio_geterror(errmsg, errmsglen, true);
            fprintf(stderr, "    %s\n", errmsg);
        }
        return -2;
    }

    result = ImageOutput_open(io, "out.exr", out_spec, OIIO_OpenMode_Create);
    if (!result) {
        fprintf(stderr, "Error opening \"out.exr\" because:\n");
        if (ImageOutput_has_error(io)) {
            ImageOutput_geterror(io, errmsg, errmsglen, true);
            fprintf(stderr, "    %s\n", errmsg);
        } else {
            fprintf(stderr, "    unknown: no errors on ImageOutput\n");
        }
    }

    // write the image
    result = ImageOutput_write_image(io,
                                     OIIO_TypeFloat,  // format
                                     data,                           // data
                                     OIIO_AutoStride,                     // xstride
                                     OIIO_AutoStride,                     // ystride
                                     OIIO_AutoStride,                     // zstride
                                     NULL,  // progress_callback
                                     NULL   // progress_callback_data
    );

    if (!result) {
        fprintf(stderr, "Error writing \"out.exr\" because:\n");
        if (ImageOutput_has_error(io)) {
            ImageOutput_geterror(io, errmsg, errmsglen, true);
            fprintf(stderr, "    %s\n", errmsg);
        } else {
            fprintf(stderr, "    unknown: no errors on ImageOutput\n");
        }
    }


    ImageInput_delete(ii);
    ImageOutput_delete(io);
    free(data);

    // re-open the output image and read the metadata to check our attributes
    // are in there
    ii = ImageInput_open("out.exr", NULL, NULL);
    if (!ii) {
        fprintf(stderr, "Could not open file \"out.exr\"\n");
        if (openimageio_haserror()) {
            openimageio_geterror(errmsg, errmsglen, true);
            fprintf(stderr, "    %s\n", errmsg);
        }
        return -1;
    }

    // Get the image dimensions
    in_spec = ImageInput_spec(ii);

    int o_int_attr = 0;
    if (ImageSpec_getattribute(in_spec, "test_int_attr",
                               OIIO_TypeInt, &o_int_attr,
                               true)) {
        printf("test_int_attr: %d\n", o_int_attr);
    } else {
        fprintf(stderr, "Could not get test_int_attr\n");
    }

    char* o_str_attr;
    if (ImageSpec_getattribute(in_spec, "test_str_attr",
                               OIIO_TypeString, &o_str_attr,
                               true)) {
        printf("test_str_attr: %s\n", o_str_attr);
    } else {
        fprintf(stderr, "Could not get test_str_attr\n");
    }

    ImageInput_delete(ii);

    return 0;
}
