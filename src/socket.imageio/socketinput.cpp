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

#include "imageio.h"
#include "socket_pvt.h"

//#define DEBUG_NO_CLIENT 1

OIIO_PLUGIN_NAMESPACE_BEGIN

// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT int socket_imageio_version = OIIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *socket_input_imageio_create () {
        return new SocketInput;
    }
    DLLEXPORT const char *socket_input_extensions[] = {
        "*m_socket", NULL
    };

OIIO_PLUGIN_EXPORTS_END



SocketInput::SocketInput()
//        : *m_socket (io)
          : m_socket(NULL),
            m_port(0),
            m_curr_tile_x(-1),
            m_curr_tile_y(-1)
{
}



bool
SocketInput::valid_file (const std::string &filename) const
{
//    // Pass it a configuration request that includes a "nowait" option
//    // so that it returns immediately rather than waiting for a socket
//    // connection that doesn't yet exist.
//    ImageSpec config;
//    config.attribute ("nowait", (int)1);
//
//    ImageSpec tmpspec;
//    bool ok = const_cast<SocketInput *>(this)->open (filename, tmpspec, config);
//    if (ok)
//        const_cast<SocketInput *>(this)->close ();
//    return ok;
    return true;
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
    std::cout << "SocketInput::open " << name << std::endl;

#ifdef DEBUG_NO_CLIENT
    std::cout << "NO Client mode" << std::endl;
    newspec.nchannels = 4;
    newspec.format = TypeDesc::FLOAT;
    newspec.x = 0;
    newspec.y = 0;
    newspec.z = 0;
    newspec.width = 640;
    newspec.height = 480;
    newspec.depth = 1;
    newspec.full_x = 0;
    newspec.full_y = 0;
    newspec.full_z = 0;
    newspec.full_width = 640;
    newspec.full_height = 480;
    newspec.full_depth = 1;
    newspec.tile_width = 64;
    newspec.tile_height = 64;
#else

    if (m_socket) {
        // if the socket is already valid, then we've previously been opened
        // TODO: check that the socket is alive
        std::cout << "socket already open" << std::endl;
        return true;
    }
    if (accept_connection (name)) {
        // If there is a nonzero "nowait" request in the configuration, just
        // return immediately.
        if (config.get_int_attribute ("nowait", 0))
            return true;

        m_socket = &ServerPool::instance()->get_socket (m_port);

        if (! (get_spec_from_client (newspec) && listen_for_header_from_client ())) {
            if (m_socket) {
                m_socket->close();
                m_socket = NULL;
                std::cout << "removed socket" << std::endl;
            }
        }
//        else {
//            std::cout << "running" << std::endl;
//                        m_thread = boost::thread(
//                                  boost::bind(&boost::asio::io_service::run, &ServerPool::instance()->get_io_service()));
////            ServerPool::instance()->get_io_service().run();
//
//        }
    } else {
        if (m_socket) {
            m_socket->close();
            m_socket = NULL;
            std::cout << "removed socket" << std::endl;
        }
        return false;
    }
    // Also send information about endianess etc.
#endif

//    std::cout << newspec.nchannels << std::endl;
//    std::cout << newspec.format << std::endl;
//    std::cout << newspec.x << std::endl;
//    std::cout << newspec.y << std::endl;
//    std::cout << newspec.width << std::endl;
//    std::cout << newspec.height << std::endl;
//    std::cout << newspec.full_x << std::endl;
//    std::cout << newspec.full_y << std::endl;
//    std::cout << newspec.full_width << std::endl;
//    std::cout << newspec.full_height << std::endl;
//    std::cout << newspec.tile_width << std::endl;
//    std::cout << newspec.tile_height << std::endl;

    m_spec = newspec;

    return true;
}



bool
SocketInput::read_native_scanline (int y, int z, void *data)
{
//    try {
//        boost::asio::read (*m_socket, buffer (reinterpret_cast<char *> (data),
//                m_spec.scanline_bytes ()));
//    } catch (boost::system::system_error &err) {
//        error ("Error while reading: %s", err.what ());
//        return false;
//    }

    return true;
}



bool
SocketInput::read_native_tile (int x, int y, int z, void *data)
{
    if (m_curr_tile_x >= 0 || m_curr_tile_y >= 0) {
    //    int size = m_spec.tile_pixels() * m_spec.nchannels; // * format.size();
        int size = m_spec.tile_bytes ();
        std::cout << "read_native_tile (" << x << ", " << y << ") size: " << size << std::endl;
        try {
            boost::asio::read (*m_socket, buffer (reinterpret_cast<char *> (data),
                    m_spec.tile_bytes ()));
        } catch (boost::system::system_error &err) {
            error ("Error while reading: %s", err.what ());
            return false;
        }
    }
    return true;
}



bool
SocketInput::close ()
{
    std::cout << "SocketInput::close" << std::endl;
    if (m_socket)
        m_socket->close();
    return true;
}



bool
SocketInput::accept_connection(const std::string &name)
{
    std::map<std::string, std::string> rest_args;
    std::string baseurl;
    rest_args["port"] = socket_pvt::default_port;
    rest_args["host"] = socket_pvt::default_host;

    std::string basename = name.substr(name.size()-7);
    if (! Strutil::get_rest_arguments (basename, baseurl, rest_args)) {
        error ("Invalid 'open ()' argument: %s", name.c_str ());
        return false;
    }

    std::cout << rest_args["host"] << std::endl;
    std::cout << rest_args["port"] << std::endl;

    m_port = atoi (rest_args["port"].c_str ());

//    try {
//        ip::tcp::endpoint endpoint (ip::tcp::v4(), port);
//        acceptor = boost::shared_ptr <ip::tcp::acceptor>
//            (new ip::tcp::acceptor (io, endpoint));
//        acceptor->accept (*m_socket);
//    } catch (boost::system::system_error &err) {
//        error ("Error while accepting: %s", err.what ());
//        return false;
//    }

    return true;
}



bool
SocketInput::get_spec_from_client (ImageSpec &spec)
{
    std::string spec_xml;
    if (get_header_from_client (spec_xml))
    {
        spec.from_xml (spec_xml.c_str ());
        return true;
    }
    return false;
}



bool
SocketInput::get_header_from_client (std::string &header)
{
    try {
        int length;

        boost::asio::read (*m_socket,
                boost::asio::buffer (reinterpret_cast<char *> (&length), sizeof (boost::uint32_t)));

        char *buf = new char[length + 1];
        boost::asio::read (*m_socket, boost::asio::buffer (buf, length));

        header = buf;
        delete [] buf;

    } catch (boost::system::system_error &err) {
        // FIXME: we have a memory leak if read fails and spec_xml is not deleted
        error ("Error while reading: %s", err.what ());
        std::cerr << "get_header_from_client: " << err.what () << std::endl;
        return false;
    }
    return true;
}

bool
SocketInput::listen_for_header_from_client ()
{
    std::cout << "listen_for_header_from_client" << std::endl;
    try {
        boost::asio::async_read (*m_socket,
                boost::asio::buffer (reinterpret_cast<char *> (&m_header_length), sizeof (boost::uint32_t)),
                boost::bind (&SocketInput::handle_read_header, this,
                                    placeholders::error));
//        m_socket->async_read_some (
//                boost::asio::buffer (reinterpret_cast<char *> (&m_header_length), sizeof (boost::uint32_t)),
//                boost::bind (&SocketInput::handle_read_header, this,
//                                    placeholders::error));

    } catch (boost::system::system_error &err) {
        error ("Error while reading: %s", err.what ());
        std::cerr << "listen_for_header_from_client: " << err.what () << std::endl;
        return false;
    }
    return true;
}



void
SocketInput::handle_read_header (const boost::system::error_code& error)
{

    if (!error) {
        std::cout << "handle_read_header: length " << m_header_length << std::endl;
//        try {
            char *buf = new char[m_header_length];
            //char buf[header_length + 1] = "";
            boost::asio::read (*m_socket, boost::asio::buffer (buf, m_header_length));

            std::string header(buf);
            //ImageSpec spc;
            //spec.from_xml (header.c_str ());
            std::map<std::string, std::string> rest_args;
            std::string baseurl;
            if (! Strutil::get_rest_arguments (header, baseurl, rest_args)) {
                //error ("Invalid header: %s", header.c_str ());
                //return;
            }
            else {
                std::cout << "TILE: " << rest_args["x"] << " " << rest_args["y"] << std::endl;
                int x = atoi (rest_args["x"].c_str ());
                int y = atoi (rest_args["y"].c_str ());
                int z = atoi (rest_args["z"].c_str ());
                m_curr_tile_x = x;
                m_curr_tile_y = y;
                if (m_tile_changed_callback) {
                    m_tile_changed_callback (m_tile_changed_callback_data, this, x, y, z);
                }
            }
            delete [] buf;

            // listen for next tile
            listen_for_header_from_client();

//        } catch (boost::system::system_error &err) {
//                // FIXME: we have a memory leak if read fails and spec_xml is not deleted
//                error ("Error while reading: %s", err.what ());
//                return;
//        }
//        boost::asio::async_read(*m_socket,
//                boost::asio::buffer (read_msg_.body(), read_msg_.body_length()),
//                boost::bind (&chat_session::handle_read_body, shared_from_this(),
//                        boost::asio::placeholders::error));
    }
    else {
        std::cout << "handle_read_header ERROR" << std::endl;
//        room_.leave (shared_from_this ());
    }
}

void
SocketInput::handle_read_data (const boost::system::error_code& error)
{
    if (!error) {
//        room_.deliver(read_msg_);
//        boost::asio::async_read(socket_,
//                boost::asio::buffer(read_msg_.data(), chat_message::header_length),
//                boost::bind(&chat_session::handle_read_header, shared_from_this(),
//                        boost::asio::placeholders::error));
    }
    else {
//      room_.leave (shared_from_this ());
    }
}

OIIO_PLUGIN_NAMESPACE_END

