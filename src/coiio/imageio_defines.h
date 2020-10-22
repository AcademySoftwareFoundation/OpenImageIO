#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <OpenImageIO/export.h>

typedef int64_t stride_t;
typedef uint64_t imagesize_t;

#define AUTOSTRIDE 0x8000000000000000L  // int64_t min

/// Pointer to a function called periodically by read_image and
/// write_image.  This can be used to implement progress feedback, etc.
/// It takes an opaque data pointer (passed to read_image/write_image)
/// and a float giving the portion of work done so far.  It returns a
/// bool, which if 'true' will STOP the read or write.
typedef bool (*ProgressCallback)(void* opaque_data, float portion_done);

enum OpenMode {
    OpenMode_Create,
    OpenMode_AppendSubimage,
    OpenMode_AppendMIPLevel
};