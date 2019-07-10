// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the socket.imageio plugin
/////////////////////////////////////////////////////////////////////////////


#pragma once

#include <map>
#include <memory>

#include <OpenImageIO/imageio.h>


// The boost::asio library uses functionality only available since Windows XP,
// thus _WIN32_WINNT must be set to _WIN32_WINNT_WINXP (0x0501) or greater.
// If _WIN32_WINNT is not defined before including the asio headers, they issue
// a message warning that _WIN32_WINNT was explicitly set to _WIN32_WINNT_WINXP.
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#    define _WIN32_WINNT 0x0501
#endif

#include <boost/asio.hpp>


OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace boost::asio;



class SocketOutput final : public ImageOutput {
public:
    SocketOutput();
    virtual ~SocketOutput() { close(); }
    virtual const char* format_name(void) const override { return "socket"; }
    virtual int supports(string_view property) const override;
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create) override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;
    virtual bool close() override;
    virtual bool copy_image(ImageInput* in) override;

private:
    int m_next_scanline;  // Which scanline is the next to write?
    io_service io;
    ip::tcp::socket socket;
    std::vector<unsigned char> m_scratch;

    bool connect_to_server(const std::string& name);
    bool send_spec_to_server(const ImageSpec& spec);
};



class SocketInput final : public ImageInput {
public:
    SocketInput();
    virtual ~SocketInput() { close(); }
    virtual const char* format_name(void) const override { return "socket"; }
    virtual bool valid_file(const std::string& filename) const override;
    virtual bool open(const std::string& name, ImageSpec& spec) override;
    virtual bool open(const std::string& name, ImageSpec& spec,
                      const ImageSpec& config) override;
    virtual bool read_native_scanline(int subimage, int miplevel, int y, int z,
                                      void* data) override;
    virtual bool read_native_tile(int subimage, int miplevel, int x, int y,
                                  int z, void* data) override;
    virtual bool close() override;

private:
    int m_next_scanline;  // Which scanline is the next to read?
    io_service io;
    ip::tcp::socket socket;
    std::shared_ptr<ip::tcp::acceptor> acceptor;

    bool accept_connection(const std::string& name);
    bool get_spec_from_client(ImageSpec& spec);

    friend class SocketOutput;
};

namespace socket_pvt {

const char default_port[] = "10110";

const char default_host[] = "127.0.0.1";

std::size_t
socket_write(ip::tcp::socket& s, TypeDesc& type, const void* data, int size);

}  // namespace socket_pvt

OIIO_PLUGIN_NAMESPACE_END
