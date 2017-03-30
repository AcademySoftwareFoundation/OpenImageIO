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

#include <OpenImageIO/imageio.h>
#include "socket_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *socket_output_imageio_create () {
        return new SocketOutput;
    }
    OIIO_EXPORT const char *socket_output_extensions[] = {
        "socket", NULL
    };

OIIO_PLUGIN_EXPORTS_END



SocketOutput::SocketOutput()
    : socket (io)
{

}



int
SocketOutput::supports (string_view feature) const
{
    return (feature == "alpha" ||
            feature == "nchannels");
}



bool
SocketOutput::open (const std::string &name, const ImageSpec &newspec,
                    OpenMode mode)
{
    if (! (connect_to_server (name) && send_spec_to_server (newspec))) {
        return false;
    }

    m_next_scanline = 0;
    m_spec = newspec;
    if (m_spec.format == TypeDesc::UNKNOWN)
        m_spec.set_format (TypeDesc::UINT8);  // Default to 8 bit channels

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
        error ("Error while writing: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while writing: unknown exception");
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

    try {
        socket_pvt::socket_write (socket, format, data, m_spec.tile_bytes ());
    } catch (boost::system::system_error &err) {
        error ("Error while writing: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while writing: unknown exception");
        return false;
    }

    return true;
}



bool
SocketOutput::close ()
{
    socket.close();
    return true;
}



bool
SocketOutput::copy_image (ImageInput *in)
{
    return true;
}



bool
SocketOutput::send_spec_to_server(const ImageSpec& spec)
{
    std::string spec_xml = spec.to_xml();
    int xml_length = spec_xml.length ();

    try {
        boost::asio::write (socket, buffer (reinterpret_cast<const char *> (&xml_length),
                sizeof (boost::uint32_t)));
        boost::asio::write (socket, buffer (spec_xml.c_str (), spec_xml.length ()));
    } catch (boost::system::system_error &err) {
        error ("Error while send_spec_to_server: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while send_spec_to_server: unknown exception");
        return false;
    }

    return true;
}



bool
SocketOutput::connect_to_server (const std::string &name)
{
    std::map<std::string, std::string> rest_args;
    std::string baseurl;
    rest_args["port"] = socket_pvt::default_port;
    rest_args["host"] = socket_pvt::default_host;
    
    if (! Strutil::get_rest_arguments (name, baseurl, rest_args)) {
        error ("Invalid 'open ()' argument: %s", name.c_str ());
        return false;
    }

    try {
        ip::tcp::resolver resolver (io);
        ip::tcp::resolver::query query (rest_args["host"].c_str (),
                rest_args["port"].c_str ());
        ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);
        ip::tcp::resolver::iterator end;

        boost::system::error_code err = error::host_not_found;
        while (err && endpoint_iterator != end) {
            socket.close ();
            socket.connect (*endpoint_iterator++, err);
        }
        if (err) {
            error ("Host \"%s\" not found", rest_args["host"].c_str ());
            return false;
        }
    } catch (boost::system::system_error &err) {
        error ("Error while connecting: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while connecting: unknown exception");
        return false;
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

