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

Session::Session(boost::asio::io_service& io_service)
    : m_socket(io_service)
{
}



tcp::socket& Session::socket()
{
    return m_socket;
}



bool
Session::get_filename (std::string &filename)
{
    try {
        int length;
        std::cout << "Session::get_filename " << std::endl;
        boost::asio::read (m_socket,
                boost::asio::buffer (reinterpret_cast<char *> (&length), sizeof (boost::uint32_t)));

        char *buf = new char[length + 1];
        boost::asio::read (m_socket, boost::asio::buffer (buf, length));

        filename = buf;
        delete [] buf;

    } catch (boost::system::system_error &err) {
        //error ("Error while reading: %s", err.what ());
        std::cerr << "get_filename: " << err.what () << std::endl;
        return false;
    }
    return true;
}



SocketServer::SocketServer(boost::asio::io_service& io_service, short port, boost::function<void(std::string&)> accept_handler)
    : m_io_service(io_service),
      m_acceptor(io_service, tcp::endpoint(tcp::v4(), port)),
      m_accept_handler(accept_handler)
{
    std::cout << "setting up accept handler " << port << std::endl;
    Session* session = new Session(m_io_service);
    m_acceptor.async_accept(session->socket(),
            boost::bind(&SocketServer::handle_accept, this, session,
                    boost::asio::placeholders::error));
}



void
SocketServer::handle_accept(Session* session, const boost::system::error_code& err)
//SocketServer::handle_accept(const boost::system::error_code& error)
{
    if (!err) {
        std::cout << "handle accept" << std::endl;
        // TODO: read filename over socket
        std::string filename;
        if (!session->get_filename (filename)) {
            // TODO: print error properly
            std::cerr << "could not get file name" << std::endl;
            delete session;
        }
        else if (SocketServerPool::instance()->m_session_map.count (filename)) {
            // TODO: print error properly
            // TODO: optionally uniquify filename
            std::cerr << "file already exists: \"" << filename << "\"" << std::endl;
            delete session;
        }
        else {
            SocketServerPool::instance()->m_session_map[filename] = session;
            m_accept_handler(filename);
        }
        session = new Session(m_io_service);
        m_acceptor.async_accept (session->socket(),
                boost::bind(&SocketServer::handle_accept, this, session,
                        boost::asio::placeholders::error));

    } else {
        delete session;
        std::cerr << "handle accept error: " << err.message() << std::endl;
    }
}



SocketServerPool* SocketServerPool::m_instance = NULL;


SocketServerPool::SocketServerPool () :
        m_io_service(new boost::asio::io_service),
        m_work(new boost::asio::io_service::work(*m_io_service))
{
}



SocketServerPool::~SocketServerPool ()
{
    m_server_list.clear ();
    m_session_map.clear ();
}



SocketServerPool*
SocketServerPool::instance ()
{
    if (!m_instance)
        m_instance = new SocketServerPool ();
    return m_instance;
}



void
SocketServerPool::destroy ()
{
    if (m_instance)
        delete m_instance;
}



bool
SocketServerPool::run ()
{
//    try {
        m_io_service->run();
//    }
//    catch (std::exception& e) {
//        std::cerr << "Exception: " << e.what() << "\n";
//        return false;
//    }
    return true;
}



void
SocketServerPool::add_server (short port, boost::function<void(std::string&)> accept_handler)
{
    server_ptr server(new SocketServer(*m_io_service, port, accept_handler));
    m_server_list.push_back(server);
}


boost::asio::io_service&
SocketServerPool::get_io_service ()
{
    boost::asio::io_service& io_service = *m_io_service;
    return io_service;
}



tcp::socket&
SocketServerPool::get_socket (const std::string& filename)
{
    std::cout << "get_socket" << std::endl;
    std::map<std::string, Session*>::iterator it;
    it = m_session_map.find(filename);
    if (it == m_session_map.end()) {
        // TODO: raise error
        std::cerr << "file not in session map " << filename << std::endl;
    }
    return it->second->socket();
}



OIIO_NAMESPACE_EXIT
}
