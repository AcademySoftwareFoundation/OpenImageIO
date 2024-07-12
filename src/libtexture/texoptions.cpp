// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <string>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/texture.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/ustring.h>


OIIO_NAMESPACE_BEGIN


namespace {  // anonymous

static const ustring wrap_type_name[] = {
    // MUST match the order of TextureOptions::Wrap
    ustring("default"),
    ustring("black"),
    ustring("clamp"),
    ustring("periodic"),
    ustring("mirror"),
    ustring("periodic_pow2"),
    ustring("periodic_sharedborder"),
    ustring()
};

static const ustringhash wrap_type_hash[] = {
    // MUST match the order of TextureOptions::Wrap
    ustringhash("default"),
    ustringhash("black"),
    ustringhash("clamp"),
    ustringhash("periodic"),
    ustringhash("mirror"),
    ustringhash("periodic_pow2"),
    ustringhash("periodic_sharedborder"),
    ustringhash()
};

}  // end anonymous namespace



Tex::Wrap
Tex::decode_wrapmode(const char* name)
{
    for (int i = 0; i < (int)Tex::Wrap::Last; ++i)
        if (!strcmp(name, wrap_type_name[i].c_str()))
            return (Wrap)i;
    return Tex::Wrap::Default;
}



Tex::Wrap
Tex::decode_wrapmode(ustring name)
{
    for (int i = 0; i < (int)Tex::Wrap::Last; ++i)
        if (name == wrap_type_name[i])
            return (Wrap)i;
    return Tex::Wrap::Default;
}

Tex::Wrap
Tex::decode_wrapmode(ustringhash name)
{
    for (int i = 0; i < (int)Tex::Wrap::Last; ++i)
        if (name == wrap_type_hash[i])
            return (Wrap)i;
    return Tex::Wrap::Default;
}



void
Tex::parse_wrapmodes(const char* wrapmodes, Tex::Wrap& swrapcode,
                     Tex::Wrap& twrapcode)
{
    char* swrap = OIIO_ALLOCA(char, strlen(wrapmodes) + 1);
    const char* twrap;
    int i;
    for (i = 0; wrapmodes[i] && wrapmodes[i] != ','; ++i)
        swrap[i] = wrapmodes[i];
    swrap[i] = 0;
    if (wrapmodes[i] == ',')
        twrap = wrapmodes + i + 1;
    else
        twrap = swrap;
    swrapcode = decode_wrapmode(swrap);
    twrapcode = decode_wrapmode(twrap);
}


OIIO_NAMESPACE_END
