#include <coiio/imageio.h>
#include <iostream>

int
main(void)
{
    // open the test image
    const char* infile = "data/checker.tif";
    ImageInput* ii     = ImageInput_open(infile, nullptr, nullptr);
    if (!ii) {
        std::cerr << "could not open " << infile << std::endl;
        if (openimageio_haserror()) {
            std::cerr << "    " << openimageio_geterror(true) << std::endl;
        }
        return -1;
    }

    // Get the image dimensions
    const ImageSpec* in_spec = ImageInput_spec(ii);
    int w                    = ImageSpec_width(in_spec);
    int h                    = ImageSpec_height(in_spec);
    std::cerr << "Dimensions are " << w << "x" << h << std::endl;

    std::cerr << "Channels are:" << std::endl;
    int nchannels = ImageSpec_nchannels(in_spec);
    for (int i = 0; i < nchannels; ++i) {
        std::cerr << "    " << ImageSpec_channel_name(in_spec, i) << std::endl;
    }

    // read image data
    float* data = new float[w * h * nchannels];
    bool result = ImageInput_read_image(ii,
                                        0,          // subimage
                                        0,          // miplevel
                                        0,          // chbegin
                                        nchannels,  // chend
                                        TypeDesc_from_string("float"),  //format
                                        data,        // pixel storage
                                        AUTOSTRIDE,  // xstride
                                        AUTOSTRIDE,  // ystride
                                        AUTOSTRIDE,  // zstride
                                        nullptr,     // progress_callback
                                        nullptr      // progress_callback_data
    );

    if (!result) {
        std::cerr << "Error loading \"" << infile << "\" because:\n";
        if (ImageInput_has_error(ii)) {
            std::cerr << "    " << ImageInput_geterror(ii) << std::endl;
        } else {
            std::cerr << "    unknown: no errors on ImageInput" << std::endl;
        }
    }

    // create a new image spec for our output image by copying the input one
    ImageSpec* out_spec = ImageSpec_copy(in_spec);

    // set a couple of test attributes
    int test_int_attr = 17;
    ImageSpec_attribute(out_spec, "test_int_attr", TypeDesc_from_string("int"),
                        &test_int_attr);

    const char* test_str_attr = "the quick brown fox...";
    ImageSpec_attribute(out_spec, "test_str_attr",
                        TypeDesc_from_string("string"), &test_str_attr);

    // create the output image
    ImageOutput* io = ImageOutput_create("out.exr", nullptr, "");
    if (!io) {
        std::cerr << "could not open out.exr" << std::endl;
        if (openimageio_haserror()) {
            std::cerr << "    " << openimageio_geterror(true) << std::endl;
        }
        return -2;
    }

    result = ImageOutput_open(io, "out.exr", out_spec, OpenMode_Create);
    if (!result) {
        std::cerr << "Error opening \"out.exr\" because:";
        if (ImageOutput_has_error(io)) {
            std::cerr << "    " << ImageOutput_geterror(io) << std::endl;
        } else {
            std::cerr << "    unknown: no errors on ImageOutput" << std::endl;
        }
    }

    // write the image
    result = ImageOutput_write_image(io,
                                     TypeDesc_from_string("float"),  // format
                                     data,                           // data
                                     AUTOSTRIDE,                     // xstride
                                     AUTOSTRIDE,                     // ystride
                                     AUTOSTRIDE,                     // zstride
                                     nullptr,  // progress_callback
                                     nullptr   // progress_callback_data
    );

    if (!result) {
        std::cerr << "Error writing \"out.exr\" because:";
        if (ImageOutput_has_error(io)) {
            std::cerr << "    " << ImageOutput_geterror(io) << std::endl;
        } else {
            std::cerr << "    unknown: no errors on ImageOutput" << std::endl;
        }
    }


    ImageInput_delete(ii);
    ImageOutput_delete(io);
    delete[] data;

    // re-open the output image and read the metadata to check our attributes
    // are in there
    ii = ImageInput_open("out.exr", nullptr, nullptr);
    if (!ii) {
        std::cerr << "could not open out.exr" << std::endl;
        if (openimageio_haserror()) {
            std::cerr << "    " << openimageio_geterror(true) << std::endl;
        }
        return -1;
    }

    // Get the image dimensions
    in_spec = ImageInput_spec(ii);

    int o_int_attr = 0;
    if (ImageSpec_getattribute(in_spec, "test_int_attr",
                               TypeDesc_from_string("int"), &o_int_attr,
                               true)) {
        std::cout << "test_int_attr: " << o_int_attr << std::endl;
    } else {
        std::cerr << "Could not get test_int_attr" << std::endl;
    }

    char* o_str_attr;
    if (ImageSpec_getattribute(in_spec, "test_str_attr",
                               TypeDesc_from_string("string"), &o_str_attr,
                               true)) {
        std::cout << "test_str_attr: " << o_str_attr << std::endl;
    } else {
        std::cerr << "Could not get test_str_attr" << std::endl;
    }

    ImageInput_delete(ii);

    return 0;
}