#pragma once

template<class T, class U>
T
pun(const U& tpp)
{
    static_assert(sizeof(T) == sizeof(U));
    static_assert(alignof(T) == alignof(U));
    T t;
    memcpy((void*)&t, (void*)&tpp, sizeof(T));
    return t;
}