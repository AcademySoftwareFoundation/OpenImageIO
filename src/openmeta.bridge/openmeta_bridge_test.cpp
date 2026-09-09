// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "openmeta_oiio.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include <OpenImageIO/unittest.h>

OIIO_NAMESPACE_USING

namespace {

void
append_ascii(std::vector<uint8_t>& bytes, std::string_view text)
{
    bytes.insert(bytes.end(), text.begin(), text.end());
}

void
append_u16le(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}

void
append_u32le(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 24));
}

void
write_u32le(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
    bytes[offset]     = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<uint8_t>(value >> 24);
}

void
append_webp_chunk(std::vector<uint8_t>& bytes, std::string_view type,
                  const std::vector<uint8_t>& payload)
{
    append_ascii(bytes, type);
    append_u32le(bytes, static_cast<uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    if (payload.size() & 1U)
        bytes.push_back(0);
}

std::vector<uint8_t>
make_tiff()
{
    std::vector<uint8_t> bytes;
    append_ascii(bytes, "II");
    append_u16le(bytes, 42);
    append_u32le(bytes, 8);
    append_u16le(bytes, 2);

    append_u16le(bytes, 0x010f);  // Make
    append_u16le(bytes, 2);       // ASCII
    append_u32le(bytes, 6);
    append_u32le(bytes, 38);

    append_u16le(bytes, 0x0112);  // Orientation
    append_u16le(bytes, 3);       // SHORT
    append_u32le(bytes, 1);
    append_u16le(bytes, 6);
    append_u16le(bytes, 0);

    append_u32le(bytes, 0);
    append_ascii(bytes, "Nikon");
    bytes.push_back(0);
    return bytes;
}

std::vector<uint8_t>
make_webp()
{
    std::vector<uint8_t> bytes;
    append_ascii(bytes, "RIFF");
    append_u32le(bytes, 0);
    append_ascii(bytes, "WEBP");
    append_webp_chunk(bytes, "VP8X", std::vector<uint8_t>(10, 0));
    append_webp_chunk(bytes, "EXIF", make_tiff());
    append_webp_chunk(bytes, "VP8 ", std::vector<uint8_t>(1, 0));
    write_u32le(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
    return bytes;
}

class CountingProxy final : public Filesystem::IOProxy {
public:
    explicit CountingProxy(std::vector<uint8_t> bytes)
        : IOProxy("openmeta-test", Read)
        , m_bytes(std::move(bytes))
    {
    }

    const char* proxytype() const override { return "openmeta-test"; }
    size_t size() const override { return m_bytes.size(); }

    size_t pread(void* buffer, size_t size, int64_t offset) override
    {
        read_requests.fetch_add(1, std::memory_order_relaxed);
        if (!buffer || offset < 0
            || static_cast<uint64_t>(offset) >= m_bytes.size())
            return 0;
        const size_t available = m_bytes.size() - static_cast<size_t>(offset);
        const size_t count     = std::min(size, available);
        std::memcpy(buffer, m_bytes.data() + static_cast<size_t>(offset),
                    count);
        return count;
    }

    std::atomic<uint32_t> read_requests { 0 };

private:
    std::vector<uint8_t> m_bytes;
};

void
test_contract()
{
    const oiio_openmeta::BridgeContract contract
        = pvt::openmeta::bridge_contract();
    OIIO_CHECK_EQUAL(contract.bridge_version,
                     oiio_openmeta::BridgeContractVersion);
    OIIO_CHECK_EQUAL(contract.host_profile_version, 1U);
    OIIO_CHECK_EQUAL(contract.compatible, 1U);
}

void
test_decode()
{
    CountingProxy proxy(make_webp());
    pvt::openmeta::DecodeRequest request;
    request.serialize_snapshot = true;
    const pvt::openmeta::DecodeResult result
        = pvt::openmeta::decode(proxy, pvt::openmeta::Format::Webp, request);

    OIIO_CHECK_ASSERT(result.ok());
    OIIO_CHECK_EQUAL(result.bridge.complete, 1U);
    OIIO_CHECK_EQUAL(result.bridge.diagnostics_needed, 0U);
    OIIO_CHECK_EQUAL(result.diagnostics.size(), 0U);
    OIIO_CHECK_EQUAL(result.attributes.get_string("Make"), "Nikon");
    OIIO_CHECK_EQUAL(result.attributes.get_int("Orientation"), 6);
    OIIO_CHECK_ASSERT(result.attributes.contains("Make"));
    OIIO_CHECK_ASSERT(!result.sources.empty());
    OIIO_CHECK_ASSERT(!result.serialized_snapshot.empty());
    OIIO_CHECK_EQUAL(result.bridge.snapshot_serialized, 1U);
    OIIO_CHECK_ASSERT(proxy.read_requests.load(std::memory_order_relaxed) > 0);
}

void
test_read_limit_diagnostic()
{
    CountingProxy proxy(make_webp());
    pvt::openmeta::DecodeRequest request;
    request.options.max_total_read_bytes = 8;
    const pvt::openmeta::DecodeResult result
        = pvt::openmeta::decode(proxy, pvt::openmeta::Format::Webp, request);

    OIIO_CHECK_FALSE(result.ok());
    OIIO_CHECK_EQUAL(result.bridge.complete, 0U);
    OIIO_CHECK_ASSERT(result.bridge.code
                      == pvt::openmeta::DecodeCode::ResourceLimit);
    OIIO_CHECK_ASSERT(result.bridge.diagnostics_needed > 0);
    OIIO_CHECK_ASSERT(!result.diagnostics.empty());
}

}  // namespace

int
main()
{
    test_contract();
    test_decode();
    test_read_limit_diagnostic();
    return unit_test_failures;
}
