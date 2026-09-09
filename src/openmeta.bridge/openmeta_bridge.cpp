// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "openmeta_bridge.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <openmeta/host_adoption.h>
#include <openmeta/interop_export.h>
#include <openmeta/metadata_transfer.h>

namespace oiio_openmeta {
namespace {

    openmeta::ContainerFormat to_openmeta_format(Format format) noexcept
    {
        switch (format) {
        case Format::Jpeg: return openmeta::ContainerFormat::Jpeg;
        case Format::Png: return openmeta::ContainerFormat::Png;
        case Format::Webp: return openmeta::ContainerFormat::Webp;
        case Format::Gif: return openmeta::ContainerFormat::Gif;
        case Format::Tiff: return openmeta::ContainerFormat::Tiff;
        case Format::Crw: return openmeta::ContainerFormat::Crw;
        case Format::Raf: return openmeta::ContainerFormat::Raf;
        case Format::X3f: return openmeta::ContainerFormat::X3f;
        case Format::Jp2: return openmeta::ContainerFormat::Jp2;
        case Format::Jxl: return openmeta::ContainerFormat::Jxl;
        case Format::Heif: return openmeta::ContainerFormat::Heif;
        case Format::Avif: return openmeta::ContainerFormat::Avif;
        case Format::Cr3: return openmeta::ContainerFormat::Cr3;
        case Format::Exr: return openmeta::ContainerFormat::Exr;
        case Format::Unknown: break;
        }
        return openmeta::ContainerFormat::Unknown;
    }

    Format from_openmeta_format(openmeta::ContainerFormat format) noexcept
    {
        switch (format) {
        case openmeta::ContainerFormat::Jpeg: return Format::Jpeg;
        case openmeta::ContainerFormat::Png: return Format::Png;
        case openmeta::ContainerFormat::Webp: return Format::Webp;
        case openmeta::ContainerFormat::Gif: return Format::Gif;
        case openmeta::ContainerFormat::Tiff: return Format::Tiff;
        case openmeta::ContainerFormat::Crw: return Format::Crw;
        case openmeta::ContainerFormat::Raf: return Format::Raf;
        case openmeta::ContainerFormat::X3f: return Format::X3f;
        case openmeta::ContainerFormat::Jp2: return Format::Jp2;
        case openmeta::ContainerFormat::Jxl: return Format::Jxl;
        case openmeta::ContainerFormat::Heif: return Format::Heif;
        case openmeta::ContainerFormat::Avif: return Format::Avif;
        case openmeta::ContainerFormat::Cr3: return Format::Cr3;
        case openmeta::ContainerFormat::Exr: return Format::Exr;
        case openmeta::ContainerFormat::Unknown: break;
        }
        return Format::Unknown;
    }

    openmeta::RandomAccessIoCode
    to_openmeta_read_code(SourceReadCode code) noexcept
    {
        switch (code) {
        case SourceReadCode::Ok: return openmeta::RandomAccessIoCode::Ok;
        case SourceReadCode::IoError:
            return openmeta::RandomAccessIoCode::IoError;
        case SourceReadCode::SourceChanged:
            return openmeta::RandomAccessIoCode::SourceChanged;
        case SourceReadCode::Cancelled:
            return openmeta::RandomAccessIoCode::Cancelled;
        }
        return openmeta::RandomAccessIoCode::IoError;
    }

    struct ReadContext final {
        const Source* source = nullptr;
    };

    openmeta::RandomAccessIoResult
    read_at(void* context, uint64_t offset,
            std::span<std::byte> destination) noexcept
    {
        const auto* read_context = static_cast<const ReadContext*>(context);
        if (!read_context || !read_context->source
            || !read_context->source->read_at) {
            return { openmeta::RandomAccessIoCode::IoError, 0 };
        }

        const SourceReadResult result
            = read_context->source->read_at(read_context->source->context,
                                            offset, destination.data(),
                                            destination.size());
        if (result.bytes_read > destination.size())
            return { openmeta::RandomAccessIoCode::IoError, 0 };
        return { to_openmeta_read_code(result.code), result.bytes_read };
    }

    bool to_size(uint64_t value, size_t* out) noexcept
    {
        if (!out || value > std::numeric_limits<size_t>::max())
            return false;
        *out = static_cast<size_t>(value);
        return true;
    }

    ElementType
    from_openmeta_element_type(openmeta::MetaElementType type) noexcept
    {
        switch (type) {
        case openmeta::MetaElementType::U8: return ElementType::U8;
        case openmeta::MetaElementType::I8: return ElementType::I8;
        case openmeta::MetaElementType::U16: return ElementType::U16;
        case openmeta::MetaElementType::I16: return ElementType::I16;
        case openmeta::MetaElementType::U32: return ElementType::U32;
        case openmeta::MetaElementType::I32: return ElementType::I32;
        case openmeta::MetaElementType::U64: return ElementType::U64;
        case openmeta::MetaElementType::I64: return ElementType::I64;
        case openmeta::MetaElementType::F32: return ElementType::F32;
        case openmeta::MetaElementType::F64: return ElementType::F64;
        case openmeta::MetaElementType::URational:
            return ElementType::URational;
        case openmeta::MetaElementType::SRational:
            return ElementType::SRational;
        }
        return ElementType::U8;
    }

    TextEncoding
    from_openmeta_text_encoding(openmeta::TextEncoding encoding) noexcept
    {
        switch (encoding) {
        case openmeta::TextEncoding::Unknown: return TextEncoding::Unknown;
        case openmeta::TextEncoding::Ascii: return TextEncoding::Ascii;
        case openmeta::TextEncoding::Utf8: return TextEncoding::Utf8;
        case openmeta::TextEncoding::Utf16LE: return TextEncoding::Utf16LE;
        case openmeta::TextEncoding::Utf16BE: return TextEncoding::Utf16BE;
        }
        return TextEncoding::Unknown;
    }

    uint64_t element_size(openmeta::MetaElementType type) noexcept
    {
        switch (type) {
        case openmeta::MetaElementType::U8:
        case openmeta::MetaElementType::I8: return 1;
        case openmeta::MetaElementType::U16:
        case openmeta::MetaElementType::I16: return 2;
        case openmeta::MetaElementType::U32:
        case openmeta::MetaElementType::I32:
        case openmeta::MetaElementType::F32: return 4;
        case openmeta::MetaElementType::U64:
        case openmeta::MetaElementType::I64:
        case openmeta::MetaElementType::F64:
        case openmeta::MetaElementType::URational:
        case openmeta::MetaElementType::SRational: return 8;
        }
        return 0;
    }

    struct ScalarStorage final {
        uint8_t u8   = 0;
        int8_t i8    = 0;
        uint16_t u16 = 0;
        int16_t i16  = 0;
        uint32_t u32 = 0;
        int32_t i32  = 0;
        uint64_t u64 = 0;
        int64_t i64  = 0;
        float f32    = 0.0f;
        double f64   = 0.0;
        URational ur;
        SRational sr;
    };

    bool make_value_view(const openmeta::MetaStore& store,
                         const openmeta::MetaValue& value, ValueView* out,
                         ScalarStorage* scalar) noexcept
    {
        if (!out || !scalar)
            return false;

        out->element_type = from_openmeta_element_type(value.elem_type);
        out->encoding     = from_openmeta_text_encoding(value.text_encoding);
        out->count        = value.count;

        switch (value.kind) {
        case openmeta::MetaValueKind::Empty:
            out->kind = ValueKind::Empty;
            return true;
        case openmeta::MetaValueKind::Scalar:
            out->kind  = ValueKind::Scalar;
            out->count = 1;
            out->size  = element_size(value.elem_type);
            switch (value.elem_type) {
            case openmeta::MetaElementType::U8:
                scalar->u8 = static_cast<uint8_t>(value.data.u64);
                out->data  = &scalar->u8;
                break;
            case openmeta::MetaElementType::U16:
                scalar->u16 = static_cast<uint16_t>(value.data.u64);
                out->data   = &scalar->u16;
                break;
            case openmeta::MetaElementType::U32:
                scalar->u32 = static_cast<uint32_t>(value.data.u64);
                out->data   = &scalar->u32;
                break;
            case openmeta::MetaElementType::U64:
                scalar->u64 = value.data.u64;
                out->data   = &scalar->u64;
                break;
            case openmeta::MetaElementType::I8:
                scalar->i8 = static_cast<int8_t>(value.data.i64);
                out->data  = &scalar->i8;
                break;
            case openmeta::MetaElementType::I16:
                scalar->i16 = static_cast<int16_t>(value.data.i64);
                out->data   = &scalar->i16;
                break;
            case openmeta::MetaElementType::I32:
                scalar->i32 = static_cast<int32_t>(value.data.i64);
                out->data   = &scalar->i32;
                break;
            case openmeta::MetaElementType::I64:
                scalar->i64 = value.data.i64;
                out->data   = &scalar->i64;
                break;
            case openmeta::MetaElementType::F32:
                std::memcpy(&scalar->f32, &value.data.f32_bits,
                            sizeof(scalar->f32));
                out->data = &scalar->f32;
                break;
            case openmeta::MetaElementType::F64:
                std::memcpy(&scalar->f64, &value.data.f64_bits,
                            sizeof(scalar->f64));
                out->data = &scalar->f64;
                break;
            case openmeta::MetaElementType::URational:
                scalar->ur = { value.data.ur.numer, value.data.ur.denom };
                out->data  = &scalar->ur;
                break;
            case openmeta::MetaElementType::SRational:
                scalar->sr = { value.data.sr.numer, value.data.sr.denom };
                out->data  = &scalar->sr;
                break;
            }
            return out->data && out->size != 0;
        case openmeta::MetaValueKind::Array:
        case openmeta::MetaValueKind::Bytes:
        case openmeta::MetaValueKind::Text: {
            const std::span<const std::byte> bytes = store.arena().span(
                value.data.span);
            uint64_t expected = value.count;
            if (value.kind == openmeta::MetaValueKind::Array) {
                const uint64_t size = element_size(value.elem_type);
                if (size == 0
                    || value.count
                           > std::numeric_limits<uint64_t>::max() / size)
                    return false;
                expected  = size * value.count;
                out->kind = ValueKind::Array;
            } else if (value.kind == openmeta::MetaValueKind::Bytes) {
                out->kind = ValueKind::Bytes;
            } else {
                out->kind = ValueKind::Text;
            }
            if (bytes.size() != expected)
                return false;
            out->data = bytes.data();
            out->size = bytes.size();
            return expected == 0 || out->data;
        }
        }
        return false;
    }

    enum class ExportPass : uint8_t {
        NativeFlatHost,
        XmpFlatHost,
        CanonicalFallback,
    };

    struct TransparentStringHash final {
        using is_transparent = void;

        size_t operator()(std::string_view value) const noexcept
        {
            return std::hash<std::string_view> {}(value);
        }

        size_t operator()(const std::string& value) const noexcept
        {
            return (*this)(std::string_view(value));
        }
    };

    struct ExportState final {
        std::span<const openmeta::Entry> entries;
        std::vector<uint8_t> entry_states;
        std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>
            names;
    };

    class ExportSink final : public openmeta::MetadataSink {
    public:
        ExportSink(const openmeta::MetaStore& store, AttributeCallback callback,
                   void* context, ExportPass pass, ExportState* state) noexcept
            : m_store(store)
            , m_callback(callback)
            , m_context(context)
            , m_pass(pass)
            , m_state(state)
        {
        }

        void on_item(const openmeta::ExportItem& item) noexcept override
        {
            if (m_rejected || m_out_of_memory || m_failed || !m_state
                || !item.entry)
                return;

            try {
                if (m_state->entries.empty()
                    || item.entry < m_state->entries.data()
                    || item.entry >= m_state->entries.data()
                                         + m_state->entries.size()) {
                    m_failed = true;
                    return;
                }
                const size_t entry_index = static_cast<size_t>(
                    item.entry - m_state->entries.data());
                if (m_state->entry_states[entry_index] != 0)
                    return;

                const bool is_xmp = item.entry->key.kind
                                    == openmeta::MetaKeyKind::XmpProperty;
                if ((m_pass == ExportPass::NativeFlatHost && is_xmp)
                    || (m_pass == ExportPass::XmpFlatHost && !is_xmp)) {
                    return;
                }

                AttributeView view;
                view.name            = item.name.data();
                view.name_size       = item.name.size();
                view.source_entry_id = static_cast<uint32_t>(entry_index);
                view.source_block_id = item.entry->origin.block;
                view.source_order    = item.entry->origin.order_in_block;
                view.flags           = static_cast<uint8_t>(item.flags);

                ScalarStorage scalar;
                if (!make_value_view(m_store, item.entry->value, &view.value,
                                     &scalar)) {
                    m_state->entry_states[entry_index] = 2;
                    ++m_skipped;
                    return;
                }

                const auto inserted = m_state->names.emplace(item.name).second;
                if (!inserted) {
                    // Retry flat-name collisions with canonical names.
                    if (m_pass == ExportPass::CanonicalFallback) {
                        m_state->entry_states[entry_index] = 2;
                        ++m_skipped;
                    }
                    return;
                }

                if (m_callback && !m_callback(m_context, &view)) {
                    m_rejected = true;
                    return;
                }
                m_state->entry_states[entry_index] = 1;
                ++m_emitted;
            } catch (const std::bad_alloc&) {
                m_out_of_memory = true;
            } catch (...) {
                m_failed = true;
            }
        }

        uint32_t emitted() const noexcept { return m_emitted; }
        uint32_t skipped() const noexcept { return m_skipped; }
        bool rejected() const noexcept { return m_rejected; }
        bool out_of_memory() const noexcept { return m_out_of_memory; }
        bool failed() const noexcept { return m_failed; }

    private:
        const openmeta::MetaStore& m_store;
        AttributeCallback m_callback = nullptr;
        void* m_context              = nullptr;
        ExportPass m_pass            = ExportPass::NativeFlatHost;
        ExportState* m_state         = nullptr;
        uint32_t m_emitted           = 0;
        uint32_t m_skipped           = 0;
        bool m_rejected              = false;
        bool m_out_of_memory         = false;
        bool m_failed                = false;
    };

    DecodeCode
    decode_code(const openmeta::ReadTransferSourceSnapshotRandomAccessResult&
                    read) noexcept
    {
        if (read.complete())
            return DecodeCode::Ok;
        if (read.code
            == openmeta::ReadTransferSourceSnapshotRandomAccessCode::
                UnsupportedFormat)
            return DecodeCode::UnsupportedFormat;
        if (read.code
            == openmeta::ReadTransferSourceSnapshotRandomAccessCode::
                ScratchTooSmall)
            return DecodeCode::ScratchTooSmall;
        if (read.code
            == openmeta::ReadTransferSourceSnapshotRandomAccessCode::
                ResidualMetadataPaths)
            return DecodeCode::Incomplete;
        if (!read.input.ok()) {
            using Code = openmeta::RandomAccessReadCode;
            if (read.input.code == Code::RequestTooLarge
                || read.input.code == Code::RequestLimitExceeded
                || read.input.code == Code::ByteLimitExceeded)
                return DecodeCode::ResourceLimit;
            return DecodeCode::InputFailure;
        }
        if (read.status == openmeta::TransferStatus::LimitExceeded)
            return DecodeCode::ResourceLimit;
        if (read.status == openmeta::TransferStatus::Unsupported)
            return DecodeCode::UnsupportedFormat;
        return DecodeCode::DecodeFailure;
    }

    DiagnosticSeverity from_openmeta_severity(
        openmeta::ReadTransferSourceDiagnosticSeverity severity) noexcept
    {
        switch (severity) {
        case openmeta::ReadTransferSourceDiagnosticSeverity::Info:
            return DiagnosticSeverity::Info;
        case openmeta::ReadTransferSourceDiagnosticSeverity::Warning:
            return DiagnosticSeverity::Warning;
        case openmeta::ReadTransferSourceDiagnosticSeverity::Error:
            return DiagnosticSeverity::Error;
        }
        return DiagnosticSeverity::Error;
    }

}  // namespace

static BridgeContract
bridge_contract_impl() noexcept
{
    const openmeta::HostAdoptionProfile profile
        = openmeta::host_adoption_profile();
    BridgeContract result;
    result.bridge_version               = BridgeContractVersion;
    result.host_profile_version         = profile.profile_version;
    result.random_access_source_version = profile.random_access_source_version;
    result.positional_snapshot_version  = profile.positional_snapshot_version;
    result.snapshot_object_version      = profile.snapshot_object_version;
    result.snapshot_serialization_version
        = profile.snapshot_serialization_version;
    result.flat_host_export_version = profile.flat_host_export_version;
    result.flat_host_import_version = profile.flat_host_import_version;
    result.read_diagnostics_version = profile.read_diagnostics_version;
    result.prepared_adapter_schema_version
        = profile.prepared_adapter_schema_version;
    result.compatible = openmeta::host_adoption_profile_matches(
                            openmeta::kHostAdoptionProfileV1)
                            ? 1
                            : 0;
    return result;
}

extern "C" void
oiio_openmeta_bridge_contract(BridgeContract* contract) noexcept
{
    if (contract)
        *contract = bridge_contract_impl();
}

static DecodeResult
decode_impl(const Source* source, Format format, const DecodeOptions* options,
            AttributeCallback attribute_callback, void* attribute_context,
            DiagnosticCallback diagnostic_callback, void* diagnostic_context,
            BlobCallback snapshot_callback, void* snapshot_context) noexcept
{
    DecodeResult summary;
    try {
        const DecodeOptions defaults;
        const DecodeOptions& requested = options ? *options : defaults;
        if (!source || source->contract_version != BridgeContractVersion
            || source->struct_size < sizeof(Source) || source->size == 0
            || !source->read_at
            || requested.contract_version != BridgeContractVersion
            || requested.struct_size < sizeof(DecodeOptions)
            || format == Format::Unknown || requested.max_blocks == 0
            || requested.max_ifds == 0 || requested.max_payload_parts == 0
            || requested.read_window_bytes == 0
            || requested.payload_scratch_bytes == 0
            || requested.compressed_scratch_bytes == 0
            || requested.value_scratch_bytes == 0
            || requested.max_read_requests == 0
            || requested.max_total_read_bytes == 0
            || requested.max_single_read_bytes == 0
            || (snapshot_callback && requested.max_serialized_snapshot == 0)) {
            summary.code = DecodeCode::InvalidArgument;
            return summary;
        }
        if (!openmeta::host_adoption_profile_matches(
                openmeta::kHostAdoptionProfileV1)) {
            summary.code = DecodeCode::IncompatibleLibrary;
            return summary;
        }

        size_t read_window_size = 0;
        size_t payload_size     = 0;
        size_t compressed_size  = 0;
        size_t value_size       = 0;
        if (!to_size(requested.read_window_bytes, &read_window_size)
            || !to_size(requested.payload_scratch_bytes, &payload_size)
            || !to_size(requested.compressed_scratch_bytes, &compressed_size)
            || !to_size(requested.value_scratch_bytes, &value_size)) {
            summary.code = DecodeCode::InvalidArgument;
            return summary;
        }

        std::vector<openmeta::ContainerBlockRef> blocks(requested.max_blocks);
        std::vector<openmeta::ExifIfdRef> ifds(requested.max_ifds);
        std::vector<uint32_t> payload_indices(requested.max_payload_parts);
        std::vector<std::byte> read_window(read_window_size);
        std::vector<std::byte> payload(payload_size);
        std::vector<std::byte> compressed(compressed_size);
        std::vector<std::byte> value(value_size);

        ReadContext read_context { source };
        const openmeta::RandomAccessSource random_source
            = openmeta::make_callback_random_access_source(
                source->size, &read_context, read_at,
                source->concurrent_reads != 0);

        openmeta::ReadTransferSourceSnapshotRandomAccessScratch scratch;
        scratch.blocks                            = blocks;
        scratch.ifds                              = ifds;
        scratch.payload_indices                   = payload_indices;
        scratch.read_window                       = read_window;
        scratch.payload                           = payload;
        scratch.compressed_payload                = compressed;
        scratch.value                             = value;
        scratch.window_options.minimum_read_bytes = read_window.size();

        openmeta::ReadTransferSourceSnapshotRandomAccessOptions read_options;
        read_options.include_pointer_tags = requested.include_pointer_tags != 0;
        read_options.decode_makernote     = requested.decode_makernote != 0;
        read_options.decode_embedded_containers
            = requested.decode_embedded_containers != 0;
        read_options.decompress            = requested.decompress != 0;
        read_options.preserve_raw_carriers = false;

        openmeta::RandomAccessReadLimits read_limits;
        read_limits.max_requests          = requested.max_read_requests;
        read_limits.max_total_bytes       = requested.max_total_read_bytes;
        read_limits.max_single_read_bytes = requested.max_single_read_bytes;

        openmeta::ReadTransferSourceSnapshotRandomAccessResult read
            = openmeta::read_transfer_source_snapshot_random_access(
                openmeta::make_random_access_source_range(random_source),
                to_openmeta_format(format), scratch, read_options, read_limits);

        summary.code                      = decode_code(read);
        summary.complete                  = read.complete() ? 1 : 0;
        summary.read_requests             = read.input.requests_issued;
        summary.bytes_requested           = read.input.bytes_requested;
        summary.bytes_completed           = read.input.bytes_completed;
        summary.payload_scratch_needed    = read.payload_scratch_needed;
        summary.compressed_scratch_needed = read.compressed_scratch_needed;
        summary.value_scratch_needed      = read.value_scratch_needed;

        const openmeta::ReadTransferSourceDiagnosticOptions diag_options {
            requested.decode_makernote != 0,
            requested.decode_embedded_containers != 0,
        };
        const openmeta::ReadTransferSourceDiagnosticsResult measured
            = openmeta::collect_read_transfer_source_diagnostics(
                read, std::span<openmeta::ReadTransferSourceDiagnostic> {},
                diag_options);
        summary.diagnostics_needed = measured.needed;
        const uint32_t diagnostic_capacity
            = std::min(measured.needed, requested.max_diagnostics);
        std::vector<openmeta::ReadTransferSourceDiagnostic> diagnostics(
            diagnostic_capacity);
        const openmeta::ReadTransferSourceDiagnosticsResult collected
            = openmeta::collect_read_transfer_source_diagnostics(read,
                                                                 diagnostics,
                                                                 diag_options);
        for (uint32_t i = 0; i < collected.written; ++i) {
            const auto& diagnostic = diagnostics[i];
            DiagnosticView view;
            view.severity       = from_openmeta_severity(diagnostic.severity);
            view.domain         = static_cast<uint8_t>(diagnostic.domain);
            view.code           = static_cast<uint16_t>(diagnostic.code);
            view.format         = from_openmeta_format(diagnostic.format);
            view.offset         = diagnostic.offset;
            view.required_bytes = diagnostic.required_bytes;
            view.count          = diagnostic.count;
            view.tag            = diagnostic.tag;
            view.input_code     = static_cast<uint8_t>(diagnostic.input_code);
            view.message = openmeta::read_transfer_source_diagnostic_message(
                diagnostic.code);
            if (diagnostic_callback
                && !diagnostic_callback(diagnostic_context, &view)) {
                summary.code = DecodeCode::DiagnosticSinkRejected;
                return summary;
            }
            ++summary.diagnostics_emitted;
        }

        if (read.snapshot.store.is_finalized()) {
            const std::span<const openmeta::Entry> entries
                = read.snapshot.store.entries();
            ExportState export_state { entries,
                                       std::vector<uint8_t>(entries.size(), 0),
                                       {} };
            export_state.names.reserve(entries.size());

            auto export_pass = [&](openmeta::ExportNameStyle style,
                                   ExportPass pass) -> bool {
                openmeta::ExportOptions export_options;
                export_options.style       = style;
                export_options.name_policy = openmeta::ExportNamePolicy::Spec;
                export_options.include_makernotes = requested.decode_makernote
                                                    != 0;
                ExportSink sink(read.snapshot.store, attribute_callback,
                                attribute_context, pass, &export_state);
                openmeta::visit_metadata(read.snapshot.store, export_options,
                                         sink);
                summary.attributes_emitted += sink.emitted();
                summary.attributes_skipped += sink.skipped();
                if (sink.rejected()) {
                    summary.code = DecodeCode::AttributeSinkRejected;
                    return false;
                }
                if (sink.out_of_memory()) {
                    summary.code = DecodeCode::OutOfMemory;
                    return false;
                }
                if (sink.failed()) {
                    summary.code = DecodeCode::InternalError;
                    return false;
                }
                return true;
            };

            // Prefer typed native names and retain XMP collisions canonically.
            if (!export_pass(openmeta::ExportNameStyle::FlatHost,
                             ExportPass::NativeFlatHost)
                || !export_pass(openmeta::ExportNameStyle::FlatHost,
                                ExportPass::XmpFlatHost)
                || !export_pass(openmeta::ExportNameStyle::Canonical,
                                ExportPass::CanonicalFallback)) {
                return summary;
            }
        }

        if (snapshot_callback && read.snapshot.store.is_finalized()) {
            openmeta::TransferSourceSnapshotIoOptions io_options;
            io_options.max_serialized_bytes = requested.max_serialized_snapshot;
            std::vector<std::byte> serialized;
            const openmeta::TransferSourceSnapshotIoResult serialized_result
                = openmeta::serialize_transfer_source_snapshot(read.snapshot,
                                                               &serialized,
                                                               io_options);
            if (serialized_result.status != openmeta::TransferStatus::Ok) {
                summary.code = DecodeCode::SnapshotFailure;
                return summary;
            }
            summary.snapshot_bytes = serialized.size();
            if (!snapshot_callback(snapshot_context, serialized.data(),
                                   serialized.size())) {
                summary.code = DecodeCode::SnapshotSinkRejected;
                return summary;
            }
            summary.snapshot_serialized = 1;
        }
        return summary;
    } catch (const std::bad_alloc&) {
        summary.code = DecodeCode::OutOfMemory;
    } catch (...) {
        summary.code = DecodeCode::InternalError;
    }
    return summary;
}

extern "C" void
oiio_openmeta_decode(const Source* source, Format format,
                     const DecodeOptions* options,
                     AttributeCallback attribute_callback,
                     void* attribute_context,
                     DiagnosticCallback diagnostic_callback,
                     void* diagnostic_context, BlobCallback snapshot_callback,
                     void* snapshot_context, DecodeResult* result) noexcept
{
    if (result)
        *result = decode_impl(source, format, options, attribute_callback,
                              attribute_context, diagnostic_callback,
                              diagnostic_context, snapshot_callback,
                              snapshot_context);
}

}  // namespace oiio_openmeta
