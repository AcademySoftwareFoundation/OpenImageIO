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

#include <OpenImageIO/imageio.h>
#include "socket_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int socket_imageio_version = OIIO_PLUGIN_VERSION;
    OIIO_EXPORT const char* socket_imageio_library_version() { return NULL; }
    OIIO_EXPORT ImageInput *socket_input_imageio_create () {
        return new SocketInput;
    }
    OIIO_EXPORT const char *socket_input_extensions[] = {
        "socket", NULL
    };

OIIO_PLUGIN_EXPORTS_END



SocketInput::SocketInput()
        : socket (io)
{
}



bool
SocketInput::valid_file (const std::string &filename) const
{
    // Pass it a configuration request that includes a "nowait" option
    // so that it returns immediately rather than waiting for a socket
    // connection that doesn't yet exist.
    ImageSpec config;
    config.attribute ("nowait", (int)1);

    ImageSpec tmpspec;
    bool ok = const_cast<SocketInput *>(this)->open (filename, tmpspec, config);
    if (ok)
        const_cast<SocketInput *>(this)->close ();
    return ok;
}



bool
SocketInput::open (const std::string &name, ImageSpec &newspec)
{
    return open (name, newspec, ImageSpec());
}



bool
SocketInput::open (const std::string &name, ImageSpec &newspec,
                   const ImageSpec &config)
{
    // If there is a nonzero "nowait" request in the configuration, just
    // return immediately.
    if (config.get_int_attribute ("nowait", 0)) {
        return false;
    }

    if (! (accept_connection (name) && get_spec_from_client (newspec))) {
        return false;
    }
    // Also send information about endianess etc.

    m_spec = newspec;

    return true;
}



bool
SocketInput::read_native_scanline (int y, int z, void *data)
{    
    try {
        boost::asio::read (socket, buffer (reinterpret_cast<char *> (data),
                m_spec.scanline_bytes ()));
    } catch (boost::system::system_error &err) {
        error ("Error while reading: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while reading: unknown exception");
        return false;
    }

    return true;
}



bool
SocketInput::read_native_tile (int x, int y, int z, void *data)
{
    try {
        boost::asio::read (socket, buffer (reinterpret_cast<char *> (data),
                m_spec.tile_bytes ()));
    } catch (boost::system::system_error &err) {
        error ("Error while reading: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while reading: unknown exception");
        return false;
    }

    return true;
}



bool
SocketInput::close ()
{
    socket.close();
    return true;
}



bool
SocketInput::accept_connection(const std::string &name)
{
    std::map<std::string, std::string> rest_args;
    std::string baseurl;
    rest_args["port"] = socket_pvt::default_port;
    rest_args["host"] = socket_pvt::default_host;

    if (! Strutil::get_rest_arguments (name, baseurl, rest_args)) {
        error ("Invalid 'open ()' argument: %s", name.c_str ());
        return false;
    }

    int port = atoi (rest_args["port"].c_str ());

    try {
        acceptor = std::shared_ptr <ip::tcp::acceptor>
            (new ip::tcp::acceptor (io, ip::tcp::endpoint (ip::tcp::v4(), port)));
        acceptor->accept (socket);
    } catch (boost::system::system_error &err) {
        error ("Error while accepting: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while accepting: unknown exception");
        return false;
    }

    return true;
}



bool
SocketInput::get_spec_from_client (ImageSpec &spec)
{
    try {
        int spec_length;
        
        boost::asio::read (socket, buffer (reinterpret_cast<char *> (&spec_length),
                sizeof (boost::uint32_t)));

        char *spec_xml = new char[spec_length + 1];
        boost::asio::read (socket, buffer (spec_xml, spec_length));

        spec.from_xml (spec_xml);
        delete [] spec_xml;
    } catch (boost::system::system_error &err) {
        error ("Error while get_spec_from_client: %s", err.what ());
        return false;
    } catch (...) {
        error ("Error while get_spec_from_client: unknown exception");
        return false;
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

