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


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the socket.imageio plugin
/////////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_SOCKET_PVT_H
#define OPENIMAGEIO_SOCKET_PVT_H

#include "imageio.h"
#include "server.h"

#include <map>

// The boost::asio library uses functionality only available since Windows XP,
// thus _WIN32_WINNT must be set to _WIN32_WINNT_WINXP (0x0501) or greater.
// If _WIN32_WINNT is not defined before including the asio headers, they issue
// a message warning that _WIN32_WINNT was explicitly set to _WIN32_WINNT_WINXP.
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#  define _WIN32_WINNT 0x0501
#endif

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/enable_shared_from_this.hpp>


OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace boost::asio;

namespace socket_pvt {

struct NativeRectangle
{
    int xbegin;
    int ybegin;
    int zbegin;
    int xend;
    int yend;
    int zend;
    int size;
};


std::size_t socket_write (ip::tcp::socket &s, TypeDesc &type, const void *data, int size);
int tile_bytes_at(const ImageSpec &spec, int x, int y, int z);
int rectangle_bytes_at(const ImageSpec &spec, int xbegin, int xend,
                       int ybegin, int yend, int zbegin, int zend);
}

class SocketOutput : public ImageOutput {
 public:
    SocketOutput ();
    virtual ~SocketOutput () { close(); }
    virtual const char * format_name (void) const { return "socket"; }
    virtual bool supports (const std::string &property) const { return false; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z,
                             TypeDesc format, const void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride);
    virtual bool write_rectangle (int xbegin, int xend, int ybegin, int yend,
                                  int zbegin, int zend, TypeDesc format,
                                  const void *data, stride_t xstride=AutoStride,
                                  stride_t ystride=AutoStride,
                                  stride_t zstride=AutoStride);
    virtual bool close ();
    virtual bool copy_image (ImageInput *in);

 private:
    int m_next_scanline;             // Which scanline is the next to write?
    boost::thread m_thread;
    io_service io;
    ip::tcp::socket m_socket;
    std::vector<unsigned char> m_scratch;

    void do_close ();
    bool connect_to_server (const std::string &name, const ImageSpec& spec);
    bool send_spec_to_server (const ImageSpec &spec);
    bool send_header_to_server (const std::string &header);
};



class SocketInput : public ImageInput {
 public:
    SocketInput ();
    virtual ~SocketInput () { close(); }
    virtual const char * format_name (void) const { return "socket"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool open (const std::string &name, ImageSpec &spec,
                       const ImageSpec &config);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);
    virtual bool close ();

 private:
    int m_next_scanline;      // Which scanline is the next to read?
    ip::tcp::socket* m_socket;
    unsigned int m_header_length;
    std::string m_filename;
    boost::thread m_thread;
    int m_curr_tile_x;
    int m_curr_tile_y;
    socket_pvt::NativeRectangle m_curr_rect;

    bool get_spec_from_client (ImageSpec &spec);
    bool get_header_from_client (std::string &header);
    bool listen_for_header_from_client ();
    void handle_read_header(const boost::system::error_code& error);

    friend class SocketOutput;
};


OIIO_PLUGIN_NAMESPACE_END


#endif /* OPENIMAGEIO_SOCKET_PVT_H */

