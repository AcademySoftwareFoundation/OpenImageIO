// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio


#include <OpenImageIO/strongparam.h>
#include <OpenImageIO/unittest.h>

using namespace OIIO;


OIIO_STRONG_PARAM_TYPE(Meters, float);
OIIO_STRONG_PARAM_TYPE(Seconds, float);


float
speed(Meters a, Seconds b)
{
    return a / b;
}



int
main(int /*argc*/, char* /*argv*/[])
{
    float s = speed(Meters(8.0f), Seconds(2.0f));
    OIIO_CHECK_EQUAL(s, 4.0f);

    return unit_test_failures;
}
