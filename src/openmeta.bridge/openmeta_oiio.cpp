// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "openmeta_oiio.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

#include <OpenImageIO/strutil.h>

OIIO_NAMESPACE_BEGIN
namespace pvt::openmeta {
namespace {

    oiio_openmeta::SourceReadResult ioproxy_read_at(void* context,
                                                    uint64_t offset,
                                                    void* destination,
                                                    uint64_t size) noexcept
    {
        auto* ioproxy = static_cast<Filesystem::IOProxy*>(context);
        if (!ioproxy || !destination
            || offset
                   > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
            || size > std::numeric_limits<size_t>::max()) {
            return { oiio_openmeta::SourceReadCode::IoError, 0 };
        }
        try {
            const size_t bytes_read
                = ioproxy->pread(destination, static_cast<size_t>(size),
                                 static_cast<int64_t>(offset));
            if (bytes_read > size)
                return { oiio_openmeta::SourceReadCode::IoError, 0 };
            return { oiio_openmeta::SourceReadCode::Ok, bytes_read };
        } catch (...) {
            return { oiio_openmeta::SourceReadCode::IoError, 0 };
        }
    }

    bool element_type(oiio_openmeta::ElementType element,
                      TypeDesc* type) noexcept
    {
        if (!type)
            return false;
        switch (element) {
        case oiio_openmeta::ElementType::U8: *type = TypeUInt8; break;
        case oiio_openmeta::ElementType::I8: *type = TypeInt8; break;
        case oiio_openmeta::ElementType::U16: *type = TypeUInt16; break;
        case oiio_openmeta::ElementType::I16: *type = TypeInt16; break;
        case oiio_openmeta::ElementType::U32: *type = TypeUInt; break;
        case oiio_openmeta::ElementType::I32: *type = TypeInt; break;
        case oiio_openmeta::ElementType::U64: *type = TypeUInt64; break;
        case oiio_openmeta::ElementType::I64: *type = TypeInt64; break;
        case oiio_openmeta::ElementType::F32: *type = TypeFloat; break;
        case oiio_openmeta::ElementType::F64:
            *type = TypeDesc(TypeDesc::DOUBLE);
            break;
        case oiio_openmeta::ElementType::URational:
            *type = TypeURational;
            break;
        case oiio_openmeta::ElementType::SRational: *type = TypeRational; break;
        }
        return type->basetype != TypeDesc::UNKNOWN;
    }

    bool text_value(const oiio_openmeta::ValueView& value, std::string* text)
    {
        if (!text || (!value.data && value.size != 0)
            || value.size > std::numeric_limits<size_t>::max())
            return false;

        const auto* bytes = static_cast<const uint8_t*>(value.data);
        const size_t size = static_cast<size_t>(value.size);
        if (size == 0) {
            text->clear();
            return true;
        }
        if (value.encoding == oiio_openmeta::TextEncoding::Ascii
            || value.encoding == oiio_openmeta::TextEncoding::Utf8) {
            size_t length = size;
            while (length && bytes[length - 1] == 0)
                --length;
            if (std::find(bytes, bytes + length, uint8_t(0)) != bytes + length)
                return false;
            text->assign(reinterpret_cast<const char*>(bytes), length);
            return true;
        }
        if (value.encoding != oiio_openmeta::TextEncoding::Utf16LE
            && value.encoding != oiio_openmeta::TextEncoding::Utf16BE)
            return false;
        if ((size & 1U) != 0)
            return false;

        std::u16string utf16;
        utf16.reserve(size / 2);
        for (size_t i = 0; i < size; i += 2) {
            uint16_t code_unit = 0;
            if (value.encoding == oiio_openmeta::TextEncoding::Utf16LE)
                code_unit = uint16_t(bytes[i]) | (uint16_t(bytes[i + 1]) << 8);
            else
                code_unit = (uint16_t(bytes[i]) << 8) | uint16_t(bytes[i + 1]);
            utf16.push_back(static_cast<char16_t>(code_unit));
        }
        while (!utf16.empty() && utf16.back() == 0)
            utf16.pop_back();
        if (std::find(utf16.begin(), utf16.end(), char16_t(0)) != utf16.end())
            return false;
        *text = Strutil::utf16_to_utf8(utf16);
        return true;
    }

    bool add_raw_value(ParamValueList* attributes, string_view name,
                       TypeDesc base_type, uint32_t count, const void* data,
                       uint64_t size)
    {
        if (!attributes || name.empty() || count == 0 || !data
            || count > static_cast<uint32_t>(std::numeric_limits<int>::max()))
            return false;

        TypeDesc type = base_type;
        if (count > 1) {
            type = TypeDesc(
                static_cast<TypeDesc::BASETYPE>(base_type.basetype),
                static_cast<TypeDesc::AGGREGATE>(base_type.aggregate),
                static_cast<TypeDesc::VECSEMANTICS>(base_type.vecsemantics),
                static_cast<int>(count));
        }
        if (size != type.size())
            return false;
        attributes->add_or_replace(
            ParamValue(name, type, 1, data, ParamValue::Copy(true)));
        return true;
    }

    struct AttributeCollector final {
        DecodeResult* result = nullptr;
        bool failed          = false;
    };

    bool
    collect_attribute(void* context,
                      const oiio_openmeta::AttributeView* attribute) noexcept
    {
        auto* collector = static_cast<AttributeCollector*>(context);
        if (!collector || !collector->result || !attribute
            || !attribute->name) {
            if (collector)
                collector->failed = true;
            return false;
        }
        try {
            if (attribute->name_size > std::numeric_limits<size_t>::max()) {
                collector->failed = true;
                return false;
            }
            const string_view name(attribute->name, attribute->name_size);
            bool mapped = false;
            if (attribute->value.kind == oiio_openmeta::ValueKind::Text) {
                if (attribute->value.encoding
                    == oiio_openmeta::TextEncoding::Unknown)
                    mapped = add_raw_value(&collector->result->attributes, name,
                                           TypeUInt8, attribute->value.count,
                                           attribute->value.data,
                                           attribute->value.size);
                else {
                    std::string text;
                    if (text_value(attribute->value, &text)) {
                        collector->result->attributes.attribute(name, text);
                        mapped = true;
                    }
                }
            } else if (attribute->value.kind
                       == oiio_openmeta::ValueKind::Bytes) {
                mapped = add_raw_value(&collector->result->attributes, name,
                                       TypeUInt8, attribute->value.count,
                                       attribute->value.data,
                                       attribute->value.size);
            } else if (attribute->value.kind == oiio_openmeta::ValueKind::Scalar
                       || attribute->value.kind
                              == oiio_openmeta::ValueKind::Array) {
                TypeDesc type;
                mapped = element_type(attribute->value.element_type, &type)
                         && add_raw_value(&collector->result->attributes, name,
                                          type, attribute->value.count,
                                          attribute->value.data,
                                          attribute->value.size);
            }

            if (!mapped) {
                ++collector->result->mapping_failures;
                return true;
            }

            SourceIdentity identity;
            identity.name     = name;
            identity.entry_id = attribute->source_entry_id;
            identity.block_id = attribute->source_block_id;
            identity.order    = attribute->source_order;
            identity.flags    = attribute->flags;
            auto found        = std::find_if(collector->result->sources.begin(),
                                             collector->result->sources.end(),
                                             [&](const SourceIdentity& item) {
                                          return item.name == identity.name;
                                             });
            if (found == collector->result->sources.end())
                collector->result->sources.emplace_back(std::move(identity));
            else
                *found = std::move(identity);
            return true;
        } catch (...) {
            collector->failed = true;
            return false;
        }
    }

    struct DiagnosticCollector final {
        DecodeResult* result = nullptr;
        bool failed          = false;
    };

    bool
    collect_diagnostic(void* context,
                       const oiio_openmeta::DiagnosticView* diagnostic) noexcept
    {
        auto* collector = static_cast<DiagnosticCollector*>(context);
        if (!collector || !collector->result || !diagnostic) {
            if (collector)
                collector->failed = true;
            return false;
        }
        try {
            Diagnostic copy;
            copy.severity       = diagnostic->severity;
            copy.domain         = diagnostic->domain;
            copy.code           = diagnostic->code;
            copy.format         = diagnostic->format;
            copy.offset         = diagnostic->offset;
            copy.required_bytes = diagnostic->required_bytes;
            copy.count          = diagnostic->count;
            copy.tag            = diagnostic->tag;
            copy.input_code     = diagnostic->input_code;
            if (diagnostic->message)
                copy.message = diagnostic->message;
            collector->result->diagnostics.emplace_back(std::move(copy));
            return true;
        } catch (...) {
            collector->failed = true;
            return false;
        }
    }

    struct SnapshotCollector final {
        DecodeResult* result = nullptr;
        bool failed          = false;
    };

    bool collect_snapshot(void* context, const void* data,
                          uint64_t size) noexcept
    {
        auto* collector = static_cast<SnapshotCollector*>(context);
        if (!collector || !collector->result || (!data && size != 0)
            || size > std::numeric_limits<size_t>::max()) {
            if (collector)
                collector->failed = true;
            return false;
        }
        try {
            if (size == 0) {
                collector->result->serialized_snapshot.clear();
                return true;
            }
            const auto* begin = static_cast<const uint8_t*>(data);
            collector->result->serialized_snapshot.assign(
                begin, begin + static_cast<size_t>(size));
            return true;
        } catch (...) {
            collector->failed = true;
            return false;
        }
    }

}  // namespace

oiio_openmeta::BridgeContract
bridge_contract() noexcept
{
    oiio_openmeta::BridgeContract result;
    oiio_openmeta::oiio_openmeta_bridge_contract(&result);
    return result;
}

DecodeResult
decode(Filesystem::IOProxy& ioproxy, Format format,
       const DecodeRequest& request) noexcept
{
    DecodeResult result;
    try {
        const size_t source_size = ioproxy.size();
        if (ioproxy.mode() != Filesystem::IOProxy::Read
            || source_size == size_t(-1) || source_size == 0) {
            result.bridge.code = DecodeCode::InvalidArgument;
            return result;
        }

        oiio_openmeta::Source source;
        source.size             = source_size;
        source.context          = &ioproxy;
        source.read_at          = ioproxy_read_at;
        source.concurrent_reads = 1;

        AttributeCollector attributes { &result };
        DiagnosticCollector diagnostics { &result };
        SnapshotCollector snapshot { &result };
        oiio_openmeta::oiio_openmeta_decode(
            &source, format, &request.options, collect_attribute, &attributes,
            collect_diagnostic, &diagnostics,
            request.serialize_snapshot ? collect_snapshot : nullptr,
            request.serialize_snapshot ? &snapshot : nullptr, &result.bridge);
        if (attributes.failed || diagnostics.failed || snapshot.failed) {
            if (result.bridge.code == DecodeCode::Ok)
                result.bridge.code = DecodeCode::InternalError;
        }
    } catch (...) {
        result.bridge.code = DecodeCode::InternalError;
    }
    return result;
}

}  // namespace pvt::openmeta
OIIO_NAMESPACE_END
