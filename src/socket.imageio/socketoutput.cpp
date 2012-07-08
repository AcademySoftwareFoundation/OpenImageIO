/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include <boost/lexical_cast.hpp>

#include "imageio.h"
#include "socket_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT ImageOutput *socket_output_imageio_create () {
        return new SocketOutput;
    }
    DLLEXPORT const char *socket_output_extensions[] = {
        "socket", NULL
    };

OIIO_PLUGIN_EXPORTS_END



SocketOutput::SocketOutput()
    : socket (io)
{

}



bool
SocketOutput::open (const std::string &name, const ImageSpec &newspec,
                    OpenMode mode)
{
    m_thread = boost::thread(boost::bind(&boost::asio::io_service::run, &io));
    if (! (connect_to_server (name, newspec) && send_spec_to_server (newspec))) {
        return false;
    }
    std::cout << "SocketOutput::open: connection successful" << std::endl;
    m_next_scanline = 0;
    m_spec = newspec;

    return true;
}



bool
SocketOutput::write_scanline (int y, int z, TypeDesc format,
                              const void *data, stride_t xstride)
{
    data = to_native_scanline (format, data, xstride, m_scratch);

    try {
        socket_pvt::socket_write (socket, format, data, m_spec.scanline_bytes ());
    } catch (boost::system::system_error &err) {
        error ("Error while reading: %s", err.what ());
        return false;
    }

    ++m_next_scanline;

    return true;
}



bool
SocketOutput::write_tile (int x, int y, int z,
                          TypeDesc format, const void *data,
                          stride_t xstride, stride_t ystride, stride_t zstride)
{
    data = to_native_tile (format, data, xstride, ystride, zstride, m_scratch);

    //ImageSpec spec(x, y, m_spec.nchannels, format);
    //std::string header = spec.to_xml ();
    std::string header = Strutil::format ("tile?x=%d&y=%d&z=%d", x, y, z);
    int size = socket_pvt::tile_bytes_at (m_spec, x, y, z);
    std::cout << "writing tile (" << x << ", " << y << ") size: " << size << " " << m_spec.tile_bytes () << std::endl;
    if (send_header_to_server (header))
    {
        try {
            socket_pvt::socket_write (socket, format, data, size);
        } catch (boost::system::system_error &err) {
            error ("Error while reading: %s", err.what ());
            return false;
        }
        return true;
    }
    return false;

}



bool
SocketOutput::write_rectangle (int xbegin, int xend, int ybegin, int yend,
                              int zbegin, int zend, TypeDesc format,
                              const void *data, stride_t xstride,
                              stride_t ystride, stride_t zstride)
{
    data = to_native_rectangle (xbegin, xend, ybegin, yend, zbegin, zend,
                                format, data, xstride, ystride, zstride, m_scratch);

    int size = socket_pvt::rectangle_bytes_at (m_spec, xbegin, xend,
                                               ybegin, yend, zbegin, zend);

    // adjust images with non-standard origins?
//    xbegin -= m_spec.x;
//    ybegin -= m_spec.y;
//    zbegin -= m_spec.z;
//    xend -= m_spec.x;
//    yend -= m_spec.y;
//    zend -= m_spec.z;
    std::string header = Strutil::format ("tile?size=%d&xmin=%d&xmax=%d" \
                                          "&ymin=%d&ymax=%d&zmin=%d&zmax=%d",
                                          size, xbegin, xend, ybegin, yend,
                                          zbegin, zend);

    std::cout << "SocketOutput::write_rectangle: (" << xbegin << ", " << ybegin << ") size: " << size << " " << m_spec.tile_bytes () << std::endl;
    if (send_header_to_server (header))
    {
        try {
            socket_pvt::socket_write (socket, format, data, size);
        } catch (boost::system::system_error &err) {
            error ("Error while reading: %s", err.what ());
            return false;
        }
        return true;
    }
    return false;

}



bool
SocketOutput::close ()
{
    std::cout << "SocketOutput::close" << std::endl;
    // TODO: notify the server that we are disconnecting
    io.post(boost::bind(&SocketOutput::do_close, this));
    m_thread.join();
    return true;
}



bool
SocketOutput::copy_image (ImageInput *in)
{
    return true;
}



void
SocketOutput::do_close ()
{
    // FIXME: this is not running
    std::cout << "SocketOutput::do_close" << std::endl;
    socket.close();
}




bool
SocketOutput::send_spec_to_server (const ImageSpec& spec)
{
    return send_header_to_server (spec.to_xml ());
}



bool
SocketOutput::send_header_to_server (const std::string &header)
{
    unsigned int length = header.length () + 1; // one extra for NULL terminator

    try {
        std::cout << "SocketOutput::send_header_to_server: sending data size: " << length << std::endl;
        // first send the size of the header
        boost::asio::write (socket,
                buffer (reinterpret_cast<const char *> (&length), sizeof (boost::uint32_t)));
        std::cout << "SocketOutput::send_header_to_server: sending data" << std::endl;
        // then send the header itself
        boost::asio::write (socket,
                buffer (header.c_str (), length));
        std::cout << "SocketOutput::send_header_to_server: done" << std::endl;
    } catch (boost::system::system_error &err) {
        error ("Error while writing: %s", err.what ());
        return false;
    }

    return true;
}



bool
SocketOutput::connect_to_server (const std::string &name, const ImageSpec& spec)
{
    std::string port = spec.get_string_attribute("port", socket_pvt::default_port);
    std::string host = spec.get_string_attribute("host", socket_pvt::default_host);

    std::cout << "host: " << host << std::endl;
    std::cout << "port: " << port << std::endl;
    try {
        ip::tcp::resolver resolver (io);
        ip::tcp::resolver::query query (host.c_str (), port.c_str ());
        ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);
        ip::tcp::resolver::iterator end;

        boost::system::error_code err = error::host_not_found;
        while (err && endpoint_iterator != end) {
            socket.close ();
            socket.connect (*endpoint_iterator++, err);
        }
        if (err) {
            error ("Host \"%s\" not found", host.c_str ());
            return false;
        }
    } catch (boost::system::system_error &err) {
        error ("Error while connecting: %s", err.what ());
        return false;
    }
    // TODO: assert name ends in .socket
    send_header_to_server (name);
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

