// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "openmeta_bridge.h"

#include <cstdint>
#include <string>
#include <vector>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/paramlist.h>

OIIO_NAMESPACE_BEGIN
namespace pvt::openmeta {

using Format        = oiio_openmeta::Format;
using DecodeCode    = oiio_openmeta::DecodeCode;
using DecodeOptions = oiio_openmeta::DecodeOptions;

struct DecodeRequest final {
    DecodeOptions options;
    bool serialize_snapshot = false;
};

struct SourceIdentity final {
    std::string name;
    uint32_t entry_id = 0xffffffffU;
    uint32_t block_id = 0xffffffffU;
    uint32_t order    = 0;
    uint8_t flags     = 0;
};

struct Diagnostic final {
    oiio_openmeta::DiagnosticSeverity severity
        = oiio_openmeta::DiagnosticSeverity::Error;
    uint8_t domain          = 0;
    uint16_t code           = 0;
    Format format           = Format::Unknown;
    uint64_t offset         = 0;
    uint64_t required_bytes = 0;
    uint32_t count          = 0;
    uint16_t tag            = 0;
    uint8_t input_code      = 0;
    std::string message;
};

struct DecodeResult final {
    oiio_openmeta::DecodeResult bridge;
    ParamValueList attributes;
    std::vector<SourceIdentity> sources;
    std::vector<Diagnostic> diagnostics;
    std::vector<uint8_t> serialized_snapshot;
    uint32_t mapping_failures = 0;

    bool ok() const noexcept { return bridge.ok() && mapping_failures == 0; }
};

oiio_openmeta::BridgeContract bridge_contract() noexcept;

DecodeResult decode(Filesystem::IOProxy& ioproxy, Format format,
                    const DecodeRequest& request = DecodeRequest {}) noexcept;

}  // namespace pvt::openmeta
OIIO_NAMESPACE_END
