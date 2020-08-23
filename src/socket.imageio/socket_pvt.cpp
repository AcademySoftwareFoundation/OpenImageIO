// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md


/////////////////////////////////////////////////////////////////////////////
// Private definitions internal to the socket.imageio plugin
/////////////////////////////////////////////////////////////////////////////



#include "socket_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace boost;
using namespace boost::asio;

namespace socket_pvt {

std::size_t
socket_write(ip::tcp::socket& s, TypeDesc& /*type*/, const void* data, int size)
{
    std::size_t bytes;

    // TODO: Translate data to correct endianess.
    bytes = write(s, buffer(reinterpret_cast<const char*>(data), size));

    return bytes;
}

}  // namespace socket_pvt

OIIO_PLUGIN_NAMESPACE_END
