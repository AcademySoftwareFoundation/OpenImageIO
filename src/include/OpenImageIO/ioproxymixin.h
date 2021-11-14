// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio

#pragma once
#define OPENIMAGEIO_IOPROXYMIXIN_H


#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_BEGIN


/// Convenience template class that can be an ImageInput or ImageOutput that
/// has extra utilities to fully support IOProxy. The way you use this is, if
/// you want to make ImageInput and ImageOutput implementations for your
/// format FMT that are IOProxy-aware, instead of inheriting directly from
/// ImageInput or ImageOutput, you can do this:
///
///      class FMTInput : public IOProxyMixin<ImageInput>
///      { ... };
///
///      class FMTOutput : public IOProxyMixin<ImageOutput>
///      { ... };
///
/// and then within those implementation classes, you can use any of the
/// IOProxyMixin utility functions.
template<class T> class IOProxyMixin : public T {
public:
    IOProxyMixin() {}
    ~IOProxyMixin() {}

    virtual int supports(string_view feature) const override
    {
        return feature == "ioproxy" || T::supports(feature);
    }

    virtual bool set_ioproxy(Filesystem::IOProxy* ioproxy) override
    {
        m_io = ioproxy;
        return true;
    }

protected:
    // The IOProxy object we will use for all I/O operations.
    Filesystem::IOProxy* m_io = nullptr;
    // The "local" proxy that we will create to use if the user didn't
    // supply a proxy for us to use.
    std::unique_ptr<Filesystem::IOProxy> m_io_local;

    // Is this file currenty opened (active proxy)?
    bool ioproxy_opened() { return m_io != nullptr; }

    // Clear the proxy ptr, and close/destroy any "local" proxy.
    void ioproxy_clear()
    {
        m_io = nullptr;
        m_io_local.reset();
    }

    // Retrieve any ioproxy request from the configuration hint spec, and make
    // `m_io` point to it. But if no IOProxy is found in the config, don't
    // overwrite one we already have.
    void ioproxy_retrieve_from_config(const ImageSpec& config)
    {
        if (auto p = config.find_attribute("oiio:ioproxy", TypeDesc::PTR))
            m_io = p->get<Filesystem::IOProxy*>();
    }

    // Presuming that `ioproxy_retrieve_from_config` has already been called,
    // if `m_io` is still not set (i.e., wasn't found in the config), open a
    // IOFile local proxy with the given read/write `mode`. Return true if a
    // proxy is set up. If it can't be done (i.e., no proxy passed, file
    // couldn't be opened), issue an error and return false.
    bool ioproxy_use_or_open(string_view name, Filesystem::IOProxy::Mode mode);

    bool ioproxy_use_or_open_for_reading(string_view name)
    {
        return ioproxy_use_or_open(name, Filesystem::IOProxy::Mode::Read);
    }

    bool ioproxy_use_or_open_for_writing(string_view name)
    {
        return ioproxy_use_or_open(name, Filesystem::IOProxy::Mode::Write);
    }

    // Helper: read from the proxy akin to fread(). Return true on success,
    // false upon failure and issue a helpful error message. NOTE: this is
    // not the same return value as std::fread, which returns the number of
    // items read.
    bool fread(void* buf, size_t itemsize, size_t nitems = 1);

    // Helper: write to the proxy akin to fwrite(). Return true on success,
    // false upon failure and issue a helpful error message. NOTE: this is
    // not the same return value as std::fwrite, which returns the number of
    // items written.
    bool fwrite(const void* buf, size_t itemsize, size_t nitems = 1);

    // Helper: seek the proxy, akin to fseek. Return true on success, false
    // upon failure and issue an error message. (NOTE: this is not the same
    // return value as std::fseek, which returns 0 on success.)
    bool fseek(int64_t pos, int origin = SEEK_SET);

    // Helper: retrieve the current position of the proxy, akin to ftell.
    int64_t ftell() { return m_io->tell(); }

    // Write a formatted string to the output proxy. Return true on success,
    // false upon failure and issue an error message.
    template<typename Str, typename... Args>
    inline bool writefmt(const Str& fmt, Args&&... args)
    {
        std::string s = Strutil::fmt::format(fmt, args...);
        return fwrite(s.data(), s.size());
    }
};

OIIO_NAMESPACE_END
