// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#pragma once

// Utility function to pun one type to another safely via memcpy
template<class T, class U>
T
bit_cast(const U& tpp)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(alignof(T) == alignof(U));
    T t;
    memcpy((void*)&t, (void*)&tpp, sizeof(T));
    return t;
}