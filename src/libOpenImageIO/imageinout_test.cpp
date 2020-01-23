// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

/////////////////////////////////////////////////////////////////////////
// Tests related to ImageInput and ImageOutput
/////////////////////////////////////////////////////////////////////////

#include <iostream>

#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;



// Generate a small test image appropriate to the given format
static ImageBuf
make_test_image(string_view formatname)
{
    ImageBuf buf;
    auto out = ImageOutput::create(formatname);
    OIIO_DASSERT(out);
    ImageSpec spec(64, 64, 4, TypeFloat);
    if (formatname == "zfile" || formatname == "fits")
        spec.nchannels = 1;  // these formats are single channel
    else if (!out->supports("alpha"))
        spec.nchannels = 3;  // this channel doesn't support alpha
    // Force a fixed datetime metadata so it can't differ between writes
    // and make different file patterns for these tests.
    spec.attribute("DateTime", "01/01/2000 00:00:00");
    buf.reset(spec);
    ImageBufAlgo::fill(buf, { 1.0f, 1.0f, 1.0f, 1.0f });
    return buf;
}



#define CHECKED(obj, call)                                                     \
    if (!obj->call) {                                                          \
        if (do_asserts)                                                        \
            OIIO_CHECK_ASSERT(false && #call);                                 \
        if (errmsg)                                                            \
            *errmsg = obj->geterror();                                         \
        else                                                                   \
            std::cout << "      " << obj->geterror() << "\n";                  \
        return false;                                                          \
    }


static bool
checked_write(ImageOutput* out, string_view filename, const ImageSpec& spec,
              TypeDesc type, const void* data, bool do_asserts = true,
              std::string* errmsg          = nullptr,
              Filesystem::IOProxy* ioproxy = nullptr)
{
    if (errmsg)
        *errmsg = "";
    std::unique_ptr<ImageOutput> out_local;
    if (!out) {
        out_local = ImageOutput::create(filename, ioproxy);
        out       = out_local.get();
    }
    OIIO_CHECK_ASSERT(out && "Failed to create output");
    if (!out) {
        if (errmsg)
            *errmsg = OIIO::geterror();
        else
            std::cout << "      " << OIIO::geterror() << "\n";
        return false;
    }

    CHECKED(out, open(filename, spec));
    CHECKED(out, write_image(type, data));
    CHECKED(out, close());
    return true;
}



static bool
checked_read(ImageInput* in, string_view filename,
             std::vector<unsigned char>& data, bool already_opened = false,
             bool do_asserts = true, std::string* errmsg = nullptr)
{
    if (errmsg)
        *errmsg = "";
    if (!already_opened) {
        ImageSpec spec;
        CHECKED(in, open(filename, spec));
    }
    data.resize(in->spec().image_pixels() * in->spec().nchannels
                * sizeof(float));
    CHECKED(in, read_image(TypeFloat, data.data()));
    CHECKED(in, close());
    return true;
}



// Helper for test_all_formats: write the pixels in buf to an in-memrory
// IOProxy, make sure it matches byte for byte the file named by disk_filename.
static bool
test_write_proxy(string_view formatname, string_view extension,
                 const std::string& disk_filename, const ImageBuf& buf)
{
    std::cout << "    Writing Proxy " << formatname << " ... ";
    std::cout.flush();
    bool ok = true;
    Sysutil::Term term(stdout);
    Filesystem::IOVecOutput outproxy;
    std::string memname = Strutil::sprintf("mem.%s", extension);
    ok = checked_write(nullptr, memname, buf.spec(), buf.spec().format,
                       buf.localpixels(), true, nullptr, &outproxy);

    // The in-memory vector we wrote should match, byte-for-byte,
    // the version we wrote to disk earlier.
    uint64_t bytes_written = Filesystem::file_size(disk_filename);
    std::vector<unsigned char> readbuf(bytes_written);
    size_t bread = Filesystem::read_bytes(disk_filename, readbuf.data(),
                                          bytes_written);
    ok           = (bread == bytes_written && outproxy.buffer() == readbuf);
    OIIO_CHECK_ASSERT(ok && "Write proxy didn't match write file");
    if (ok)
        std::cout << term.ansi("green", "OK\n");
    else {
        Strutil::printf("Read size=%d write size=%d\n", bread, bytes_written);
    }
    return ok;
}



// Helper for test_all_formats: read the pixels of the given disk file into
// a buffer, then use an IOProxy to read the "file" from the buffer, and
// the pixels ought to match those of ImageBuf buf.
static bool
test_read_proxy(string_view formatname, string_view extension,
                const std::string& disk_filename, const ImageBuf& buf)
{
    bool ok = true;
    Sysutil::Term term(stdout);
    uint64_t bytes_written = Filesystem::file_size(disk_filename);
    std::vector<unsigned char> readbuf(bytes_written);
    Filesystem::read_bytes(disk_filename, readbuf.data(), bytes_written);
    std::cout << "    Reading Proxy " << formatname << " ... ";
    std::cout.flush();
    Filesystem::IOMemReader inproxy(readbuf);
    std::string memname = Strutil::sprintf("mem.%s", extension);
    auto in             = ImageInput::open(memname, nullptr, &inproxy);
    OIIO_CHECK_ASSERT(in && "Failed to open input with proxy");
    if (in) {
        readbuf.clear();
        std::vector<unsigned char> readbuf2;
        ok = checked_read(in.get(), memname, readbuf2, true);
        ok &= memcmp(readbuf2.data(), buf.localpixels(), readbuf2.size()) == 0;
        OIIO_CHECK_ASSERT(ok && "Read proxy didn't match original");
        if (ok)
            std::cout << term.ansi("green", "OK\n");
    } else {
        std::cout << "Error was: " << OIIO::geterror() << "\n";
    }
    return ok;
}



// Test writer's ability to detect and recover from errors when asked to
// write an unwriteable file (such as in a nonexistent directory).
static bool
test_write_unwriteable(string_view extension, const ImageBuf& buf)
{
    bool ok = true;
    Sysutil::Term term(stdout);
    std::string bad_filename = Strutil::sprintf("bad/bad.%s", extension);
    std::cout << "    Writing bad to " << bad_filename << " ... ";
    auto badout = ImageOutput::create(bad_filename);
    if (badout) {
        std::string errmsg;
        ok = checked_write(badout.get(), bad_filename, buf.spec(),
                           buf.spec().format, buf.localpixels(),
                           /*do_asserts=*/false, &errmsg);
        if (!ok)
            std::cout << term.ansi("green", "OK") << " ("
                      << errmsg.substr(0, 60) << ")\n";
        else
            OIIO_CHECK_ASSERT(0 && "Bad write should not have 'succeeded'");
    } else {
        OIIO_CHECK_ASSERT(badout);
        ok = false;
    }
    return ok;
}



static void
test_all_formats()
{
    Sysutil::Term term(stdout);
    std::cout << "Testing formats:\n";
    auto all_fmts
        = Strutil::splitsv(OIIO::get_string_attribute("extension_list"), ";");
    for (auto& e : all_fmts) {
        auto fmtexts           = Strutil::splitsv(e, ":");
        string_view formatname = fmtexts[0];
        // Skip "formats" that aren't amenable to this kind of testing
        if (formatname == "null" || formatname == "socket")
            continue;
        // Field3d very finicky. Skip for now. FIXME?
        if (formatname == "field3d")
            continue;
        auto extensions = Strutil::splitsv(fmtexts[1], ",");
        bool ok         = true;

        //
        // Try writing the file
        //
        std::string filename = Strutil::sprintf("imageinout_test-%s.%s",
                                                formatname, extensions[0]);
        auto out             = ImageOutput::create(filename);
        if (!out) {
            std::cout << "  [skipping " << formatname << " -- no writer]\n";
            (void)OIIO::geterror();  // discard error
            continue;
        }
        bool ioproxy_write_supported = out->supports("ioproxy");
        std::cout << "  " << formatname << " ("
                  << Strutil::join(extensions, ", ") << "):\n";

        ImageBuf buf             = make_test_image(formatname);
        const float* orig_pixels = (const float*)buf.localpixels();

        std::cout << "    Writing " << filename << " ... ";
        ok = checked_write(out.get(), filename, buf.spec(), buf.spec().format,
                           orig_pixels);
        if (ok)
            std::cout << term.ansi("green", "OK\n");

        //
        // Try reading the file, and make sure it matches what we wrote
        //
        std::vector<unsigned char> pixels;
        auto in = ImageInput::create(filename);
        OIIO_CHECK_ASSERT(in && "Could not create reader");
        bool ioproxy_read_supported = in && in->supports("ioproxy");
        if (in) {
            std::cout << "    Reading " << filename << " ... ";
            ok = checked_read(in.get(), filename, pixels);
            if (!ok)
                continue;
            ok = memcmp(orig_pixels, pixels.data(), pixels.size()) == 0;
            OIIO_CHECK_ASSERT(ok && "Failed read/write comparison");
            if (ok)
                std::cout << term.ansi("green", "OK\n");
        } else {
            (void)OIIO::geterror();  // discard error
        }
        if (!ok)
            continue;

        //
        // If this format supports proxies, round trip through memory
        //
        if (ioproxy_write_supported)
            test_write_proxy(formatname, extensions[0], filename, buf);
        if (ioproxy_read_supported)
            test_read_proxy(formatname, extensions[0], filename, buf);

        //
        // Test what happens when we write to an unwriteable or nonexistent
        // directory. It should not crash! But appropriately return some
        // error.
        //
        test_write_unwriteable(extensions[0], buf);

        Filesystem::remove(filename);
    }
    std::cout << "\n";
}



// This tests a particular troublesome case where we got the logic wrong.
// Read 1-channel float exr into 4-channel uint8 buffer with 4-byte xstride.
// The correct behavior is to translate the one channel from float to uint8
// and put it in channel 0, leaving channels 1-3 untouched. The bug was that
// because the buffer stride and native stride were both 4 bytes, it was
// incorrectly doing a straight data copy.
void
test_read_tricky_sizes()
{
    // Make 4x4 1-channel float source image, value 0.5, write it.
    char srcfilename[] = "tmp_f1.exr";
    ImageSpec fsize1(4, 4, 1, TypeFloat);
    ImageBuf src(fsize1);
    ImageBufAlgo::fill(src, 0.5f);
    src.write(srcfilename);

    // Make a 4x4 4-channel uint8 buffer, initialize with 0
    unsigned char buf[4][4][4];
    memset(buf, 0, 4 * 4 * 4);

    // Read in, make sure it's right, several different ways
    {
        auto imgin = ImageInput::open(srcfilename);
        imgin->read_image(TypeUInt8, buf, 4 /* xstride */);
        OIIO_CHECK_EQUAL(int(buf[0][0][0]), 128);
        OIIO_CHECK_EQUAL(int(buf[0][0][1]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][2]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][3]), 0);
    }
    {
        memset(buf, 0, 4 * 4 * 4);
        auto imgin = ImageInput::open(srcfilename);
        imgin->read_scanlines(0, 0, 0, 4, 0, 0, 4, TypeUInt8, buf,
                              /*xstride=*/4);
        OIIO_CHECK_EQUAL(int(buf[0][0][0]), 128);
        OIIO_CHECK_EQUAL(int(buf[0][0][1]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][2]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][3]), 0);
    }
    {
        memset(buf, 0, 4 * 4 * 4);
        auto imgin = ImageInput::open(srcfilename);
        for (int y = 0; y < 4; ++y)
            imgin->read_scanline(y, 0, TypeUInt8, buf, /*xstride=*/4);
        OIIO_CHECK_EQUAL(int(buf[0][0][0]), 128);
        OIIO_CHECK_EQUAL(int(buf[0][0][1]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][2]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][3]), 0);
    }
    // And repeat for tiled
    src.set_write_tiles(2, 2);
    src.write(srcfilename);
    {
        memset(buf, 0, 4 * 4 * 4);
        auto imgin = ImageInput::open(srcfilename);
        imgin->read_image(TypeUInt8, buf, 4 /* xstride */);
        OIIO_CHECK_EQUAL(int(buf[0][0][0]), 128);
        OIIO_CHECK_EQUAL(int(buf[0][0][1]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][2]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][3]), 0);
    }
    {
        memset(buf, 0, 4 * 4 * 4);
        auto imgin = ImageInput::open(srcfilename);
        imgin->read_tiles(0, 0, 0, 4, 0, 4, 0, 1, 0, 4, TypeUInt8, buf,
                          /*xstride=*/4);
        OIIO_CHECK_EQUAL(int(buf[0][0][0]), 128);
        OIIO_CHECK_EQUAL(int(buf[0][0][1]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][2]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][3]), 0);
    }
    {
        memset(buf, 0, 4 * 4 * 4);
        auto imgin = ImageInput::open(srcfilename);
        imgin->read_tile(0, 0, 0, TypeUInt8, buf, /*xstride=*/4);
        OIIO_CHECK_EQUAL(int(buf[0][0][0]), 128);
        OIIO_CHECK_EQUAL(int(buf[0][0][1]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][2]), 0);
        OIIO_CHECK_EQUAL(int(buf[0][0][3]), 0);
    }

    // Clean up
    Filesystem::remove(srcfilename);
}



int
main(int /*argc*/, char* /*argv*/[])
{
    test_all_formats();
    test_read_tricky_sizes();

    return unit_test_failures;
}
