/*
  Copyright 2019 Larry Gritz and the other authors and contributors.
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



static bool
checked_write(ImageOutput* out, string_view filename, const ImageSpec& spec,
              TypeDesc type, const void* data)
{
    bool ok = out->open(filename, spec);
    OIIO_CHECK_ASSERT(ok && "Unable to open");
    if (!ok) {
        std::cout << "      " << out->geterror() << "\n";
        return ok;
    }
    ok = out->write_image(type, data);
    OIIO_CHECK_ASSERT(ok && "Unable to write_image");
    if (!ok) {
        std::cout << "      " << out->geterror() << "\n";
        return ok;
    }
    ok = out->close();
    OIIO_CHECK_ASSERT(ok && "Unable to close");
    if (!ok) {
        std::cout << "      " << out->geterror() << "\n";
        return ok;
    }
    return ok;
}



static bool
checked_read(ImageInput* in, string_view filename,
             std::vector<unsigned char>& data, bool already_opened = false)
{
    bool ok = true;
    if (!already_opened) {
        ImageSpec spec;
        ok = in->open(filename, spec);
        OIIO_CHECK_ASSERT(ok && "Unable to open");
        if (!ok) {
            std::cout << "      " << in->geterror() << "\n";
            return ok;
        }
    }
    data.resize(in->spec().image_pixels() * in->spec().nchannels
                * sizeof(float));
    ok = in->read_image(TypeFloat, data.data());
    OIIO_CHECK_ASSERT(ok && "Unable to read_image");
    if (!ok) {
        std::cout << "      " << in->geterror() << "\n";
        return ok;
    }
    ok = in->close();
    OIIO_CHECK_ASSERT(ok && "Unable to close");
    if (!ok) {
        std::cout << "      " << in->geterror() << "\n";
        return ok;
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
            continue;
        }
        bool ioproxy_write_supported = out->supports("ioproxy");
        std::cout << "  " << formatname << " ("
                  << Strutil::join(extensions, ", ") << "):\n";

        ImageBuf buf;
        if (formatname == "zfile" || formatname == "fits")
            buf.reset(ImageSpec(64, 64, 1, TypeFloat));
        else if (!out->supports("alpha"))
            buf.reset(ImageSpec(64, 64, 3, TypeFloat));
        else
            buf.reset(ImageSpec(64, 64, 4, TypeFloat));
        ImageBufAlgo::fill(buf, { 1.0f, 1.0f, 1.0f, 1.0f });
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
        }
        if (!ok)
            continue;

        //
        // If this format supports proxies, round trip through memory
        //
        std::string memname = Strutil::sprintf("mem.%s", extensions[0]);
        if (ioproxy_write_supported) {
            // Write the file again, to in-memory vector
            Filesystem::IOVecOutput outproxy;
            ImageSpec proxyspec(buf.spec());
            void* ptr = &outproxy;
            proxyspec.attribute("oiio:ioproxy", TypeDesc::PTR, &ptr);
            auto out = ImageOutput::create(memname);
            OIIO_CHECK_ASSERT(out && "Failed to create output for proxy");
            if (out) {
                std::cout << "    Writing Proxy " << formatname << " ... ";
                ok = checked_write(out.get(), memname, proxyspec,
                                   buf.spec().format, orig_pixels);
            }
            out.reset();

            // The in-memory vector we wrote should match, byte-for-byte,
            // the version we wrote to disk earlier.
            uint64_t bytes_written = Filesystem::file_size(filename);
            std::vector<unsigned char> readbuf(bytes_written);
            size_t bread = Filesystem::read_bytes(filename, readbuf.data(),
                                                  bytes_written);
            ok = (bread == bytes_written && outproxy.buffer() == readbuf);
            OIIO_CHECK_ASSERT(ok && "Write proxy didn't match write file");
            if (ok)
                std::cout << term.ansi("green", "OK\n");
            else {
                Strutil::printf("Read size=%d write size=%d\n", bread,
                                bytes_written);
            }
        }

        if (ioproxy_read_supported) {
            // Now read from memory, and again it ought to match the pixels
            // we started with.
            uint64_t bytes_written = Filesystem::file_size(filename);
            std::vector<unsigned char> readbuf(bytes_written);
            Filesystem::read_bytes(filename, readbuf.data(), bytes_written);
            std::cout << "    Reading Proxy " << formatname << " ... ";
            std::cout.flush();
            Filesystem::IOMemReader inproxy(readbuf);
            void* ptr = &inproxy;
            ImageSpec config;
            config.attribute("oiio:ioproxy", TypeDesc::PTR, &ptr);
            auto in = ImageInput::open(memname, &config);
            OIIO_CHECK_ASSERT(in && "Failed to open input with proxy");
            if (in) {
                readbuf.clear();
                std::vector<unsigned char> readbuf2;
                ok = checked_read(in.get(), memname, readbuf2, true);
                ok &= memcmp(readbuf2.data(), orig_pixels, readbuf2.size())
                      == 0;
                OIIO_CHECK_ASSERT(ok && "Read proxy didn't match original");
                if (ok)
                    std::cout << term.ansi("green", "OK\n");
            } else {
                std::cout << "Error was: " << OIIO::geterror() << "\n";
            }
        }

        if (ok)
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
main(int argc, char** argv)
{
    test_all_formats();
    test_read_tricky_sizes();

    return unit_test_failures;
}
