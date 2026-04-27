// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_types.h"

using namespace OIIO;

namespace Imiv {

size_t
upload_data_type_size(UploadDataType type)
{
    switch (type) {
    case UploadDataType::UInt8: return 1;
    case UploadDataType::UInt16: return 2;
    case UploadDataType::UInt32: return 4;
    case UploadDataType::Half: return 2;
    case UploadDataType::Float: return 4;
    case UploadDataType::Double: return 8;
    default: break;
    }
    return 0;
}

const char*
upload_data_type_name(UploadDataType type)
{
    switch (type) {
    case UploadDataType::UInt8: return "u8";
    case UploadDataType::UInt16: return "u16";
    case UploadDataType::UInt32: return "u32";
    case UploadDataType::Half: return "half";
    case UploadDataType::Float: return "float";
    case UploadDataType::Double: return "double";
    default: break;
    }
    return "unknown";
}

TypeDesc
upload_data_type_to_typedesc(UploadDataType type)
{
    switch (type) {
    case UploadDataType::UInt8: return TypeUInt8;
    case UploadDataType::UInt16: return TypeUInt16;
    case UploadDataType::UInt32: return TypeUInt32;
    case UploadDataType::Half: return TypeHalf;
    case UploadDataType::Float: return TypeFloat;
    case UploadDataType::Double: return TypeDesc::DOUBLE;
    default: break;
    }
    return TypeUnknown;
}

bool
map_spec_type_to_upload(TypeDesc spec_type, UploadDataType& upload_type,
                        TypeDesc& read_format)
{
    const TypeDesc::BASETYPE base = static_cast<TypeDesc::BASETYPE>(
        spec_type.basetype);
    if (base == TypeDesc::UINT8) {
        upload_type = UploadDataType::UInt8;
        read_format = TypeUInt8;
        return true;
    }
    if (base == TypeDesc::UINT16) {
        upload_type = UploadDataType::UInt16;
        read_format = TypeUInt16;
        return true;
    }
    if (base == TypeDesc::UINT32) {
        upload_type = UploadDataType::UInt32;
        read_format = TypeUInt32;
        return true;
    }
    if (base == TypeDesc::HALF) {
        upload_type = UploadDataType::Half;
        read_format = TypeHalf;
        return true;
    }
    if (base == TypeDesc::FLOAT) {
        upload_type = UploadDataType::Float;
        read_format = TypeFloat;
        return true;
    }
    if (base == TypeDesc::DOUBLE) {
        upload_type = UploadDataType::Double;
        read_format = TypeDesc::DOUBLE;
        return true;
    }
    return false;
}

}  // namespace Imiv
