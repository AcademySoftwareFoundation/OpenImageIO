/*
  Copyright 2011 Larry Gritz and the other authors and contributors.
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

#include <boost/bind.hpp>

#include "strutil.h"
#include "server.h"

OIIO_NAMESPACE_ENTER
{

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_service& io_service, short port, boost::function<void(std::string&)> accept_handler)
    : m_socket(io_service),
      m_io_service(io_service),
      m_acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
      m_accept_handler(accept_handler)
{
    m_filename = Strutil::format ("sockethandle?port=%d.socket", port);
    std::cout << "setting up accept handler " << port << std::endl;
    m_acceptor.async_accept(m_socket,
            boost::bind(&Server::handle_accept, this,
                    boost::asio::placeholders::error));
}



void
Server::handle_accept(const boost::system::error_code& error)
{
    if (!error) {
        std::cout << "handle accept" << std::endl;

        m_accept_handler(m_filename);
        // what to do wit the current image
        //callback(newimage);
//        std::cout << "setting up new accept handler " << std::endl;
//        newimage = new ImageBuf(m_filename);
//        //m_socket = tcp::socket(m_io_service);
//        m_acceptor.async_accept(m_socket,
//                boost::bind(&Server::handle_accept, this, newimage,
//                        boost::asio::placeholders::error));
    } else {
        std::cout << "handle accept error: " << std::endl;
    }
}



tcp::socket&
Server::get_socket()
{
    return m_socket;
}


ServerPool* ServerPool::m_instance = NULL;


ServerPool::ServerPool () : m_io_service()
{
}



ServerPool::~ServerPool ()
{
    m_server_list.clear ();
    m_server_map.clear ();
}



ServerPool*
ServerPool::instance ()
{
    if (!m_instance)
        m_instance = new ServerPool ();
    return m_instance;
}



void
ServerPool::destroy ()
{
    if (m_instance)
        delete m_instance;
}



bool
ServerPool::run ()
{
//    try {
        m_io_service.run();
//    }
//    catch (std::exception& e) {
//        std::cerr << "Exception: " << e.what() << "\n";
//        return false;
//    }
    return true;
}



void
ServerPool::add_server (short port, boost::function<void(std::string&)> accept_handler)
{
    server_ptr server(new Server(m_io_service, port, accept_handler));
    m_server_list.push_back(server);
    m_server_map[port] = server;
}


boost::asio::io_service&
ServerPool::get_io_service ()
{
    return m_io_service;
}



tcp::socket&
ServerPool::get_socket (short port)
{
    return m_server_map[port]->get_socket();
}



OIIO_NAMESPACE_EXIT
}
