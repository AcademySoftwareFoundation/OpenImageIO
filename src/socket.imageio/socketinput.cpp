// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <OpenImageIO/imageio.h>

#include "socket_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int socket_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
socket_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT ImageInput*
socket_input_imageio_create()
{
    return new SocketInput;
}

OIIO_EXPORT const char* socket_input_extensions[] = { "socket", nullptr };

OIIO_PLUGIN_EXPORTS_END



SocketInput::SocketInput()
    : socket(io)
{
}



bool
SocketInput::valid_file(const std::string& filename) const
{
    // Pass it a configuration request that includes a "nowait" option
    // so that it returns immediately rather than waiting for a socket
    // connection that doesn't yet exist.
    ImageSpec config;
    config.attribute("nowait", (int)1);

    ImageSpec tmpspec;
    bool ok = const_cast<SocketInput*>(this)->open(filename, tmpspec, config);
    if (ok)
        const_cast<SocketInput*>(this)->close();
    return ok;
}



bool
SocketInput::open(const std::string& name, ImageSpec& newspec)
{
    return open(name, newspec, ImageSpec());
}



bool
SocketInput::open(const std::string& name, ImageSpec& newspec,
                  const ImageSpec& config)
{
    // If there is a nonzero "nowait" request in the configuration, just
    // return immediately.
    if (config.get_int_attribute("nowait", 0)) {
        return false;
    }

    if (!(accept_connection(name) && get_spec_from_client(newspec))) {
        return false;
    }
    // Also send information about endianess etc.

    m_spec = newspec;

    return true;
}



bool
SocketInput::read_native_scanline(int subimage, int miplevel, int /*y*/,
                                  int /*z*/, void* data)
{
    lock_guard lock(m_mutex);
    if (!seek_subimage(subimage, miplevel))
        return false;
    try {
        boost::asio::read(socket, buffer(reinterpret_cast<char*>(data),
                                         m_spec.scanline_bytes()));
    } catch (boost::system::system_error& err) {
        errorf("Error while reading: %s", err.what());
        return false;
    } catch (...) {
        errorf("Error while reading: unknown exception");
        return false;
    }

    return true;
}



bool
SocketInput::read_native_tile(int subimage, int miplevel, int /*x*/, int /*y*/,
                              int /*z*/, void* data)
{
    lock_guard lock(m_mutex);
    if (!seek_subimage(subimage, miplevel))
        return false;
    try {
        boost::asio::read(socket, buffer(reinterpret_cast<char*>(data),
                                         m_spec.tile_bytes()));
    } catch (boost::system::system_error& err) {
        errorf("Error while reading: %s", err.what());
        return false;
    } catch (...) {
        errorf("Error while reading: unknown exception");
        return false;
    }

    return true;
}



bool
SocketInput::close()
{
    socket.close();
    return true;
}



bool
SocketInput::accept_connection(const std::string& name)
{
    std::map<std::string, std::string> rest_args;
    std::string baseurl;
    rest_args["port"] = socket_pvt::default_port;
    rest_args["host"] = socket_pvt::default_host;

    if (!Strutil::get_rest_arguments(name, baseurl, rest_args)) {
        errorf("Invalid 'open ()' argument: %s", name);
        return false;
    }

    int port = Strutil::stoi(rest_args["port"]);

    try {
        acceptor = std::shared_ptr<ip::tcp::acceptor>(
            new ip::tcp::acceptor(io, ip::tcp::endpoint(ip::tcp::v4(), port)));
        acceptor->accept(socket);
    } catch (boost::system::system_error& err) {
        errorf("Error while accepting: %s", err.what());
        return false;
    } catch (...) {
        errorf("Error while accepting: unknown exception");
        return false;
    }

    return true;
}



bool
SocketInput::get_spec_from_client(ImageSpec& spec)
{
    try {
        int spec_length;

        boost::asio::read(socket, buffer(reinterpret_cast<char*>(&spec_length),
                                         sizeof(boost::uint32_t)));

        char* spec_xml = new char[spec_length + 1];
        boost::asio::read(socket, buffer(spec_xml, spec_length));

        spec.from_xml(spec_xml);
        delete[] spec_xml;
    } catch (boost::system::system_error& err) {
        errorf("Error while get_spec_from_client: %s", err.what());
        return false;
    } catch (...) {
        errorf("Error while get_spec_from_client: unknown exception");
        return false;
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END
