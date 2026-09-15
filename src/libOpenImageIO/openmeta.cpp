// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "openmeta.h"

#include <OpenImageIO/strutil.h>

#include <openmeta/exif_tiff_decode.h>
#include <openmeta/interop_export.h>
#include <openmeta/xmp_decode.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

OIIO_NAMESPACE_BEGIN
namespace pvt {
namespace {

    constexpr uint32_t max_entries    = 4096;
    constexpr size_t max_bytes        = 64 * 1024 * 1024;
    constexpr size_t max_output_bytes = 1024 * 1024;

    string_view arena_string(const openmeta::MetaStore& store,
                             openmeta::ByteSpan span)
    {
        auto bytes = store.arena().span(span);
        return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
    }

    struct Value {
        TypeDesc type;
        uint16_t tiff_type                       = 0;
        uint32_t count                           = 0;
        alignas(8) std::array<uint8_t, 8> scalar = {};
        cspan<uint8_t> bytes;

        template<class T> void set(T v)
        {
            memcpy(scalar.data(), &v, sizeof(v));
        }

        Value(const openmeta::MetaStore& store, const openmeta::MetaValue& v)
        {
            using E = openmeta::MetaElementType;
            using K = openmeta::MetaValueKind;
            count   = v.count;
            // Only access the active scalar union member. Arrays and text use span.
            const auto& data = v;
            switch (v.elem_type) {
            case E::U8:
                type      = TypeUInt8;
                tiff_type = 1;
                if (v.kind == K::Scalar)
                    set(uint8_t(data.data.u64));
                break;
            case E::I8:
                type      = TypeInt8;
                tiff_type = 6;
                if (v.kind == K::Scalar)
                    set(int8_t(data.data.i64));
                break;
            case E::U16:
                type      = TypeUInt16;
                tiff_type = 3;
                if (v.kind == K::Scalar)
                    set(uint16_t(data.data.u64));
                break;
            case E::I16:
                type      = TypeInt16;
                tiff_type = 8;
                if (v.kind == K::Scalar)
                    set(int16_t(data.data.i64));
                break;
            case E::U32:
                type      = TypeUInt;
                tiff_type = 4;
                if (v.kind == K::Scalar)
                    set(uint32_t(data.data.u64));
                break;
            case E::I32:
                type      = TypeInt;
                tiff_type = 9;
                if (v.kind == K::Scalar)
                    set(int32_t(data.data.i64));
                break;
            case E::U64:
                type      = TypeUInt64;
                tiff_type = 16;
                if (v.kind == K::Scalar)
                    set(data.data.u64);
                break;
            case E::I64:
                type      = TypeInt64;
                tiff_type = 17;
                if (v.kind == K::Scalar)
                    set(data.data.i64);
                break;
            case E::F32:
                type      = TypeFloat;
                tiff_type = 11;
                if (v.kind == K::Scalar)
                    set(data.data.f32_bits);
                break;
            case E::F64:
                type      = TypeDesc::DOUBLE;
                tiff_type = 12;
                if (v.kind == K::Scalar)
                    set(data.data.f64_bits);
                break;
            case E::URational:
                type      = TypeURational;
                tiff_type = 5;
                if (v.kind == K::Scalar)
                    set(data.data.ur);
                break;
            case E::SRational:
                type      = TypeRational;
                tiff_type = 10;
                if (v.kind == K::Scalar)
                    set(data.data.sr);
                break;
            }
            if (v.kind == K::Scalar) {
                count = 1;
                bytes = { scalar.data(), type.size() };
            } else if (v.kind != K::Empty) {
                auto span = store.arena().span(v.data.span);
                bytes     = { reinterpret_cast<const uint8_t*>(span.data()),
                              span.size() };
                if (v.kind == K::Text || v.kind == K::Bytes) {
                    type      = TypeUInt8;
                    tiff_type = v.kind == K::Text ? 2 : 7;
                }
            }
        }
    };

    bool add_value(ImageSpec& spec, string_view name,
                   const openmeta::MetaValue& v, const Value& value)
    {
        if (v.kind == openmeta::MetaValueKind::Text
            && v.text_encoding != openmeta::TextEncoding::Unknown) {
            std::string text;
            if (v.text_encoding == openmeta::TextEncoding::Ascii
                || v.text_encoding == openmeta::TextEncoding::Utf8) {
                text.assign(reinterpret_cast<const char*>(value.bytes.data()),
                            value.bytes.size());
            } else {
                if (value.bytes.size() % 2)
                    return false;
                std::u16string utf16;
                for (size_t i = 0; i < value.bytes.size(); i += 2) {
                    uint16_t a = value.bytes[i], b = value.bytes[i + 1];
                    utf16.push_back(v.text_encoding
                                            == openmeta::TextEncoding::Utf16LE
                                        ? a | (b << 8)
                                        : (a << 8) | b);
                }
                text = Strutil::utf16_to_utf8(utf16);
            }
            while (!text.empty() && text.back() == 0)
                text.pop_back();
            if (text.find('\0') != std::string::npos)
                return false;
            spec.attribute(name, text);
            return true;
        }
        if (!value.count || value.count > max_output_bytes
            || value.bytes.size() != value.type.size() * value.count)
            return false;
        TypeDesc type = value.type;
        if (value.count > 1)
            type.arraylen = value.count;
        spec.attribute(name, type, value.bytes.data());
        return true;
    }

    // Resolve namespace URIs, not the arbitrary prefixes chosen by the writer.
    string_view xmp_prefix(string_view uri)
    {
        static const std::pair<string_view, string_view> prefixes[] = {
            { "http://ns.adobe.com/tiff/1.0/", "tiff:" },
            { "http://ns.adobe.com/exif/1.0/", "exif:" },
            { "http://ns.adobe.com/exif/1.0/aux/", "aux:" },
            { "http://ns.adobe.com/xap/1.0/", "xmp:" },
            { "http://ns.adobe.com/xap/1.0/mm/", "xmpMM:" },
            { "http://ns.adobe.com/xap/1.0/rights/", "xmpRights:" },
            { "http://purl.org/dc/elements/1.1/", "dc:" },
            { "http://ns.adobe.com/photoshop/1.0/", "photoshop:" },
            { "http://ns.adobe.com/camera-raw-settings/1.0/", "crs:" },
            { "http://ns.google.com/photos/1.0/panorama/", "GPano:" },
            { "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/", "Iptc4xmpCore:" },
            { "http://iptc.org/std/Iptc4xmpExt/2008-02-29/", "Iptc4xmpExt:" },
        };
        for (auto& item : prefixes)
            if (item.first == uri)
                return item.second;
        return {};
    }

    class Collector final : public openmeta::MetadataSink {
    public:
        const openmeta::MetaStore& store;
        const ImageSpec& original;
        ImageSpec decoded;
        std::vector<std::string> canonical;
        std::vector<bool> visited;
        bool names_only     = true;
        bool failed         = false;
        size_t output_bytes = 0;

        Collector(const openmeta::MetaStore& s, const ImageSpec& spec)
            : store(s)
            , original(spec)
            , canonical(s.entries().size())
            , visited(s.entries().size(), false)
        {
        }

        void on_item(const openmeta::ExportItem& item) noexcept override
        {
            if (failed || !item.entry)
                return;
            try {
                size_t index = item.entry - store.entries().data();
                if (names_only) {
                    canonical[index] = item.name;
                    return;
                }
                if (visited[index])
                    return;
                visited[index] = true;
                const auto& e  = *item.entry;
                Value value(store, e.value);
                output_bytes += value.bytes.size() + canonical[index].size();
                if (output_bytes > max_output_bytes) {
                    failed = true;
                    return;
                }
                string_view name(item.name.data(), item.name.size());
                string_view full(canonical[index]);
                if (e.key.kind == openmeta::MetaKeyKind::ExifTag) {
                    auto ifd = arena_string(store, e.key.data.exif_tag.ifd);
                    const bool plain_text
                        = e.value.kind != openmeta::MetaValueKind::Text
                          || e.value.text_encoding
                                 == openmeta::TextEncoding::Ascii
                          || e.value.text_encoding
                                 == openmeta::TextEncoding::Utf8;
                    if (plain_text
                        && openmetadata_exif_attribute(
                            ifd, e.key.data.exif_tag.tag, value.tiff_type,
                            value.count, value.bytes, decoded))
                        return;
                    // Thumbnail IFDs must not overwrite the primary image tags.
                    if (ifd != "ifd0" && ifd != "exififd" && ifd != "gpsifd"
                        && !Strutil::starts_with(ifd, "mk_"))
                        name = full;
                } else if (e.key.kind == openmeta::MetaKeyKind::XmpProperty) {
                    const auto& key = e.key.data.xmp_property;
                    auto prefix     = xmp_prefix(
                        arena_string(store, key.schema_ns));
                    auto path     = arena_string(store, key.property_path);
                    auto bracket  = path.find('[');
                    bool sequence = bracket != string_view::npos;
                    bool simple   = path.find('/') == string_view::npos;
                    auto suffix   = sequence ? path.substr(bracket)
                                             : string_view();
                    if (sequence
                        && Strutil::starts_with(suffix, "[@xml:lang=")) {
                        simple &= suffix == "[@xml:lang=x-default]";
                        sequence = false;
                    }
                    if (!prefix.empty() && simple) {
                        std::string xmlname = std::string(prefix)
                                              + std::string(
                                                  path.substr(0, bracket));
                        auto text = arena_string(store, e.value.data.span);
                        ImageSpec candidate;
                        if (openmetadata_xmp_attribute(xmlname, text, sequence,
                                                       candidate)) {
                            bool conflict = false;
                            for (const auto& p : candidate.extra_attribs) {
                                const auto* previous = original.find_attribute(
                                    p.name());
                                if (previous) {
                                    conflict |= previous->get_string(0)
                                                != p.get_string(0);
                                } else {
                                    openmetadata_xmp_attribute(xmlname, text,
                                                               sequence,
                                                               decoded);
                                }
                            }
                            if (!conflict)
                                return;
                        }
                    }
                    name = full;
                }
                if (decoded.find_attribute(name))
                    name = full;
                if (!decoded.find_attribute(name))
                    add_value(decoded, name, e.value, value);
            } catch (...) {
                failed = true;
            }
        }
    };

    bool export_attributes(openmeta::MetaStore& store, ImageSpec& spec,
                           bool exif)
    {
        if (store.resource_limit_exceeded())
            return false;
        store.finalize();
        Collector collector(store, spec);
        openmeta::ExportOptions options;
        options.name_policy = openmeta::ExportNamePolicy::Spec;
        options.style       = openmeta::ExportNameStyle::Canonical;
        openmeta::visit_metadata(store, options, collector);
        collector.names_only = false;
        options.style        = openmeta::ExportNameStyle::FlatHost;
        openmeta::visit_metadata(store, options, collector);
        // FlatHost intentionally omits properties it cannot flatten safely.
        options.style = openmeta::ExportNameStyle::Canonical;
        openmeta::visit_metadata(store, options, collector);
        if (collector.failed)
            return false;
        ImageSpec result;
        result.extra_attribs = spec.extra_attribs;
        for (const auto& p : collector.decoded.extra_attribs)
            result.extra_attribs.add_or_replace(p);
        if (exif) {
            const auto* cs = result.find_attribute("Exif:ColorSpace");
            if (cs && cs->get_int() != 0xffff)
                result.set_colorspace("srgb_rec709_scene");
        }
        spec.extra_attribs = std::move(result.extra_attribs);
        return true;
    }

}  // namespace

bool
openmetadata_decode_exif(cspan<uint8_t> bytes, ImageSpec& spec) noexcept
{
    if (bytes.size() > max_bytes)
        return false;
    try {
        openmeta::MetaStore store;
        store.constrain_resources(max_entries, max_bytes);
        openmeta::ExifDecodeOptions options;
        options.decode_makernote         = true;
        options.include_pointer_tags     = false;
        options.limits.max_total_entries = max_entries;
        options.limits.max_value_bytes   = max_output_bytes;
        std::array<openmeta::ExifIfdRef, 128> ifds;
        auto result = openmeta::decode_exif_tiff(
            { reinterpret_cast<const std::byte*>(bytes.data()), bytes.size() },
            store, ifds, options);
        return result.status == openmeta::ExifDecodeStatus::Ok
               && export_attributes(store, spec, true);
    } catch (...) {
        return false;
    }
}

bool
openmetadata_decode_xmp(string_view bytes, ImageSpec& spec) noexcept
{
    if (bytes.size() > max_bytes)
        return false;
    try {
        openmeta::MetaStore store;
        store.constrain_resources(max_entries, max_bytes);
        openmeta::XmpDecodeOptions options;
        options.limits.max_depth             = 64;
        options.limits.max_properties        = max_entries;
        options.limits.max_value_bytes       = max_output_bytes;
        options.limits.max_total_value_bytes = max_output_bytes;
        auto result                          = openmeta::decode_xmp_packet(
            { reinterpret_cast<const std::byte*>(bytes.data()), bytes.size() },
            store, openmeta::EntryFlags::None, options);
        return result.status == openmeta::XmpDecodeStatus::Ok
               && export_attributes(store, spec, false);
    } catch (...) {
        return false;
    }
}

}  // namespace pvt
OIIO_NAMESPACE_END
