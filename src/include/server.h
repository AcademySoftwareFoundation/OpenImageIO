#ifndef SERVER_H_
#define SERVER_H_

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

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>

#include "dassert.h"
#include "typedesc.h"
#include "strutil.h"
#include "fmath.h"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

#include "imagebuf.h"
#include "strutil.h"

OIIO_NAMESPACE_ENTER
{

using boost::asio::ip::tcp;

typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

const char default_port[] = "10110";
const char default_host[] = "127.0.0.1";

/// Represents a successful server connection.
/// Its main purpose is to hold the data socket and to get the incoming file name
class Session
{
public:
    Session(boost::asio::io_service& io_service);
    bool get_filename (std::string &filename);
    tcp::socket& socket();

private:
    tcp::socket m_socket;
};

/// A simple socket server that listens for incoming SocketOutput request and
/// triggers a callback when one is received.
class SocketServer
{
public:
    SocketServer (boost::asio::io_service& io_service, short port, boost::function<void(std::string&)> accept_callback);

    void handle_accept (Session* session, const boost::system::error_code& error);
    //void handle_accept (const boost::system::error_code& error);

private:
    boost::asio::io_service& m_io_service;
    tcp::acceptor m_acceptor;
    boost::function<void(std::string&)> m_accept_handler;
};

typedef boost::shared_ptr<SocketServer> server_ptr;

/// A singleton class for managing servers listening for incoming SocketOutput requests.
class SocketServerPool
{
public:
    static SocketServerPool *instance ();
    static void destroy ();
    bool run ();
    // Start a server listening on the given port. It will call accept_callback
    // with the name of the file that the connected SocketOutput will send.
    // This callback should ultimately create a SocketInput class.
    void add_server (short port, boost::function<void(std::string&)> accept_callback);
    boost::asio::io_service& get_io_service ();

    /// Given a filename return the socket used for data transfer.
    /// This is used internally by SocketInput.
    tcp::socket& get_socket (const std::string& filename);
    bool close_socket (const std::string& filename);

private:
    SocketServerPool ();
    ~SocketServerPool ();

    // Make delete private and unimplemented in order to prevent apps
    // from calling it.  Instead, they should call SocketServerPool::destroy().
    void operator delete (void * /*todel*/) { }

    static SocketServerPool* m_instance;
    io_service_ptr m_io_service;
    work_ptr m_work;
    std::vector<server_ptr> m_server_list;
    std::map<std::string, Session*> m_session_map;

    friend class SocketServer;
};

OIIO_NAMESPACE_EXIT
}


#endif
