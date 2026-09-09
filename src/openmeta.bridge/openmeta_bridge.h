// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(OIIO_OPENMETA_BRIDGE_STATIC)
#    define OIIO_OPENMETA_BRIDGE_API
#elif defined(_WIN32)
#    if defined(OIIO_OPENMETA_BRIDGE_EXPORTS)
#        define OIIO_OPENMETA_BRIDGE_API __declspec(dllexport)
#    else
#        define OIIO_OPENMETA_BRIDGE_API __declspec(dllimport)
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    define OIIO_OPENMETA_BRIDGE_API __attribute__((visibility("default")))
#else
#    define OIIO_OPENMETA_BRIDGE_API
#endif

namespace oiio_openmeta {

inline constexpr uint32_t BridgeContractVersion = 1;

enum class Format : uint8_t {
    Unknown,
    Jpeg,
    Png,
    Webp,
    Gif,
    Tiff,
    Crw,
    Raf,
    X3f,
    Jp2,
    Jxl,
    Heif,
    Avif,
    Cr3,
    Exr,
};

enum class SourceReadCode : uint8_t {
    Ok,
    IoError,
    SourceChanged,
    Cancelled,
};

struct SourceReadResult final {
    SourceReadCode code = SourceReadCode::Ok;
    uint64_t bytes_read = 0;
};

using ReadAtCallback = SourceReadResult (*)(void* context, uint64_t offset,
                                            void* destination,
                                            uint64_t size) noexcept;

struct Source final {
    uint32_t contract_version = BridgeContractVersion;
    uint32_t struct_size      = sizeof(Source);
    uint64_t size             = 0;
    void* context             = nullptr;
    ReadAtCallback read_at    = nullptr;
    uint8_t concurrent_reads  = 0;
};

enum class ValueKind : uint8_t {
    Empty,
    Scalar,
    Array,
    Bytes,
    Text,
};

enum class ElementType : uint8_t {
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
    F32,
    F64,
    URational,
    SRational,
};

enum class TextEncoding : uint8_t {
    Unknown,
    Ascii,
    Utf8,
    Utf16LE,
    Utf16BE,
};

struct URational final {
    uint32_t numerator   = 0;
    uint32_t denominator = 1;
};

struct SRational final {
    int32_t numerator   = 0;
    int32_t denominator = 1;
};

struct ValueView final {
    ValueKind kind           = ValueKind::Empty;
    ElementType element_type = ElementType::U8;
    TextEncoding encoding    = TextEncoding::Unknown;
    uint32_t count           = 0;
    const void* data         = nullptr;
    uint64_t size            = 0;
};

struct AttributeView final {
    const char* name         = nullptr;
    uint64_t name_size       = 0;
    uint32_t source_entry_id = 0xffffffffU;
    uint32_t source_block_id = 0xffffffffU;
    uint32_t source_order    = 0;
    uint8_t flags            = 0;
    ValueView value;
};

using AttributeCallback = bool (*)(void* context,
                                   const AttributeView* attribute) noexcept;

enum class DiagnosticSeverity : uint8_t {
    Info,
    Warning,
    Error,
};

struct DiagnosticView final {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    uint8_t domain              = 0;
    uint16_t code               = 0;
    Format format               = Format::Unknown;
    uint64_t offset             = 0;
    uint64_t required_bytes     = 0;
    uint32_t count              = 0;
    uint16_t tag                = 0;
    uint8_t input_code          = 0;
    const char* message         = nullptr;
};

using DiagnosticCallback = bool (*)(void* context,
                                    const DiagnosticView* diagnostic) noexcept;
using BlobCallback       = bool (*)(void* context, const void* data,
                                    uint64_t size) noexcept;

struct DecodeOptions final {
    uint32_t contract_version = BridgeContractVersion;
    uint32_t struct_size      = sizeof(DecodeOptions);

    uint32_t max_blocks        = 256;
    uint32_t max_ifds          = 512;
    uint32_t max_payload_parts = 1024;
    uint32_t max_diagnostics   = 64;

    uint64_t read_window_bytes        = 4ULL * 1024ULL;
    uint64_t payload_scratch_bytes    = 2ULL * 1024ULL * 1024ULL;
    uint64_t compressed_scratch_bytes = 2ULL * 1024ULL * 1024ULL;
    uint64_t value_scratch_bytes      = 1ULL * 1024ULL * 1024ULL;

    uint32_t max_read_requests       = 65536;
    uint64_t max_total_read_bytes    = 64ULL * 1024ULL * 1024ULL;
    uint64_t max_single_read_bytes   = 16ULL * 1024ULL * 1024ULL;
    uint64_t max_serialized_snapshot = 64ULL * 1024ULL * 1024ULL;

    uint8_t include_pointer_tags       = 1;
    uint8_t decode_makernote           = 0;
    uint8_t decode_embedded_containers = 0;
    uint8_t decompress                 = 1;
};

enum class DecodeCode : uint16_t {
    Ok,
    InvalidArgument,
    IncompatibleLibrary,
    UnsupportedFormat,
    InputFailure,
    ScratchTooSmall,
    ResourceLimit,
    DecodeFailure,
    Incomplete,
    AttributeSinkRejected,
    DiagnosticSinkRejected,
    SnapshotFailure,
    SnapshotSinkRejected,
    OutOfMemory,
    InternalError,
};

struct DecodeResult final {
    DecodeCode code                    = DecodeCode::InvalidArgument;
    uint8_t complete                   = 0;
    uint8_t snapshot_serialized        = 0;
    uint32_t attributes_emitted        = 0;
    uint32_t attributes_skipped        = 0;
    uint32_t diagnostics_emitted       = 0;
    uint32_t diagnostics_needed        = 0;
    uint64_t read_requests             = 0;
    uint64_t bytes_requested           = 0;
    uint64_t bytes_completed           = 0;
    uint64_t payload_scratch_needed    = 0;
    uint64_t compressed_scratch_needed = 0;
    uint64_t value_scratch_needed      = 0;
    uint64_t snapshot_bytes            = 0;

    bool ok() const noexcept { return code == DecodeCode::Ok; }
};

struct BridgeContract final {
    uint32_t bridge_version                  = 0;
    uint32_t host_profile_version            = 0;
    uint32_t random_access_source_version    = 0;
    uint32_t positional_snapshot_version     = 0;
    uint32_t snapshot_object_version         = 0;
    uint32_t snapshot_serialization_version  = 0;
    uint32_t flat_host_export_version        = 0;
    uint32_t flat_host_import_version        = 0;
    uint32_t read_diagnostics_version        = 0;
    uint32_t prepared_adapter_schema_version = 0;
    uint8_t compatible                       = 0;
};

extern "C" OIIO_OPENMETA_BRIDGE_API void
oiio_openmeta_bridge_contract(BridgeContract* contract) noexcept;

extern "C" OIIO_OPENMETA_BRIDGE_API void oiio_openmeta_decode(
    const Source* source, Format format, const DecodeOptions* options,
    AttributeCallback attribute_callback, void* attribute_context,
    DiagnosticCallback diagnostic_callback, void* diagnostic_context,
    BlobCallback snapshot_callback, void* snapshot_context,
    DecodeResult* result) noexcept;

}  // namespace oiio_openmeta
