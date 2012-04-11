#ifndef OPENIMAGEIO_IV_UTILS_H
#define OPENIMAGEIO_IV_UTILS_H

#include "fmath.h"

OIIO_NAMESPACE_ENTER
{

/// Round up to the next power of 2
/// TODO: This should be optimized to use bit arithmetic on the ieee float
/// representation.  Once optimized and tested, move to fmath.h

inline float
pow2roundupf (float f)
{
    float logval = logf (f) / logf (2.0f);
    logval += 1e-6f; // add floating point slop. this supports [0.00012207,8192]
    return powf (2.0f, ceilf (logval));
}

/// Round down to the next power of 2
/// TODO: This should be optimized to use bit arithmetic on the ieee float
/// representation.  Once optimized and tested, move to fmath.h

inline float
pow2rounddownf (float f)
{
    float logval = logf (f) / logf (2.0f);
    logval -= 1e-6f; // add floating point slop. this supports [0.00012207,8192]
    return powf (2.0f, floorf (logval));
}

}
OIIO_NAMESPACE_EXIT

#endif // OPENIMAGEIO_IV_UTILS_H
