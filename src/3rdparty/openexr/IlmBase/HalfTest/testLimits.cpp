#include <testLimits.h>
#include "halfLimits.h"
#include <iostream>
#include <assert.h>


using namespace std;

namespace {

float
mypow (int x, int y)
{
    bool negative = false;

    if (y < 0)
    {
	negative = true;
	y = -y;
    }

    float z = 1;

    while (y > 0)
    {
	z *= x;
	y -= 1;
    }

    if (negative)
	z = 1 / z;

    return z;
}

} // namespace


void
testLimits()
{
    cout << "values in std::numeric_limits<half>\n";

    cout << "min_exponent\n";

    {
	half h (mypow (2, numeric_limits<half>::min_exponent - 1));
	assert (h.isNormalized());
    }

    {
	half h (mypow (2, numeric_limits<half>::min_exponent - 2));
	assert (h.isDenormalized());
    }

    cout << "max_exponent\n";

    {
	half h (mypow (2, numeric_limits<half>::max_exponent - 1));
	assert (h.isNormalized());
    }

    {
	half h (mypow (2, numeric_limits<half>::max_exponent));
	assert (h.isInfinity());
    }

    cout << "min_exponent10\n";

    {
	half h (mypow (10, numeric_limits<half>::min_exponent10));
	assert (h.isNormalized());
    }

    {
	half h (mypow (10, numeric_limits<half>::min_exponent10 - 1));
	assert (h.isDenormalized());
    }

    cout << "max_exponent10\n";

    {
	half h (mypow (10, numeric_limits<half>::max_exponent10));
	assert (h.isNormalized());
    }

    {
	half h (mypow (10, numeric_limits<half>::max_exponent10 + 1));
	assert (h.isInfinity());
    }

    cout << "ok\n\n" << flush;

}
