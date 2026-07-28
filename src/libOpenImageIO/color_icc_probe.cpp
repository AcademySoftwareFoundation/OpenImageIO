// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// ICC profile identification (decode an embedded ICC profile through OCIO's
// matrix/TRC reader and fingerprint it against the interop identities
// registry) and the mastering display volume (SMPTE ST 2086) derivation.
// Split out of color_ocio.cpp; see color_ocio_pvt.h for the shared internal
// declarations.

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <OpenImageIO/strutil.h>

#include "color_ocio_pvt.h"



// The built-in interop identities config and the interoperability
// assertion/bootstrap machinery below touch ColorConfig::Impl, which lives in
// the ABI-versioned v3_1 namespace -- so they must too. The OIIO_API pvt
// shims that expose them are declared (by color_pvt.h) in the library's
// "current" namespace and are defined further down in a separate
// OIIO_NAMESPACE_BEGIN block; those reach back here with explicit v3_1::
// qualification.
OIIO_NAMESPACE_3_1_BEGIN

// ---------------------------------------------------------------------------
// ICC profile identification -- decode an embedded ICC profile through
// OCIO's matrix/TRC ICC FileTransform reader inside a throwaway in-memory
// probe config, then fingerprint the decoded transform against the interop
// identities registry above. Mirror-inside: the reader's acceptance rules
// (colorant matrix + per-channel tone curves, hardcoded Bradford D50->D65
// adaptation) are OCIO's, never re-derived here.
// ---------------------------------------------------------------------------

namespace {

// Serves the embedded ICC blob to OCIO's ICC FileTransform reader without
// touching disk. The probe config is built in code (never parsed), so
// getConfigData() is intentionally empty -- OCIO only consults the proxy
// for LUT/file reads once setConfigIOProxy is attached to an
// already-constructed config. The config carries exactly one file (the
// virtual profile name), which OCIO absolutizes against the config's
// search path before asking the proxy -- so serve the single blob for any
// non-null request rather than matching the (absolutized) filename.
class IccBlobProxy final : public OCIO::ConfigIOProxy {
public:
    IccBlobProxy(cspan<uint8_t> blob, std::string hash)
        : m_blob(blob.begin(), blob.end())
        , m_hash(std::move(hash))
    {
    }

    std::vector<uint8_t> getLutData(const char* filepath) const override
    {
        if (filepath)
            return m_blob;
        throw OCIO::Exception("IccBlobProxy: unexpected LUT request");
    }
    std::string getConfigData() const override { return {}; }
    std::string getFastLutFileHash(const char* filepath) const override
    {
        return filepath ? m_hash : std::string();
    }

private:
    std::vector<uint8_t> m_blob;
    std::string m_hash;
};


// Build the throwaway probe config for one ICC profile: a display-referred
// space "icc_probe" whose to_reference is a FileTransform on the profile
// set INVERSE (OCIO's ICC FileTransform forward maps reference(PCS) ->
// device; inverting makes to_reference DECODE device code values into the
// CIE-XYZ-D65 display reference), plus an identity "cie_xyz_d65" space
// carrying the display interchange role so the fingerprint probe values
// are already in the reference and normalization is a no-op.
//
// The per-profile virtual filename embeds the content identifier -- this
// is load-bearing, NOT cosmetic: OCIO's process-global GetFastFileHash
// cache (PathUtils.cpp) keys purely on the resolved filename and never
// re-consults the ConfigIOProxy, and a config's processor cache ID hashes
// its serialization (which embeds the src). A shared filename would make
// every probe config collide on both keys, handing back the FIRST
// profile's processor for every later profile in the process (e.g. an
// undecodable cLUT "decoding" as a previously-seen sRGB). A content-unique
// name keeps distinct profiles distinct.
OCIO::ConstConfigRcPtr
make_icc_probe_config(cspan<uint8_t> iccdata)
{
    const std::string hash = OIIO::pvt::icc_profile_identifier(iccdata);
    // Start from CreateRaw's minimal working config (current version, "raw"
    // space + default role already valid). validate() rejects an empty
    // search_path when a FileTransform is present, so set one -- the actual
    // bytes come from the ConfigIOProxy, never this path.
    auto cfg = OCIO::Config::CreateRaw()->createEditableCopy();
    cfg->setName("oiio-icc-probe");
    cfg->setSearchPath(".");
    // Every processor built against the probe is a one-shot; a per-config
    // processor cache would only add mutex serialization and retained
    // processors.
    cfg->setProcessorCacheFlags(OCIO::PROCESSOR_CACHE_OFF);

    auto xyz = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_DISPLAY);
    xyz->setName("cie_xyz_d65");
    cfg->addColorSpace(xyz);
    cfg->setRole(OCIO::ROLE_INTERCHANGE_DISPLAY, "cie_xyz_d65");

    // OCIO validation requires a view transform whenever display-referred
    // spaces exist; bridge scene AP0 <-> CIE-XYZ-D65 with the standard
    // utility builtin (also wires the scene interchange, matching the
    // registry topology).
    auto ap0 = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_SCENE);
    ap0->setName("lin_ap0");
    cfg->addColorSpace(ap0);
    cfg->setRole(OCIO::ROLE_INTERCHANGE_SCENE, "lin_ap0");
    auto bridge = OCIO::ViewTransform::Create(OCIO::REFERENCE_SPACE_SCENE);
    bridge->setName("scene_to_display_bridge");
    auto toxyz = OCIO::BuiltinTransform::Create();
    toxyz->setStyle("UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD");
    bridge->setTransform(toxyz, OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE);
    cfg->addViewTransform(bridge);
    cfg->setDefaultViewTransformName("scene_to_display_bridge");

    auto probe = OCIO::ColorSpace::Create(OCIO::REFERENCE_SPACE_DISPLAY);
    probe->setName("icc_probe");
    auto ft = OCIO::FileTransform::Create();
    ft->setSrc(("embedded_" + hash + ".icc").c_str());
    ft->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
    probe->setTransform(ft, OCIO::COLORSPACE_DIR_TO_REFERENCE);
    cfg->addColorSpace(probe);

    cfg->setConfigIOProxy(std::make_shared<IccBlobProxy>(iccdata, hash));
    cfg->validate();
    return cfg;
}

}  // namespace

// Core of pvt::identify_icc_profile() (the pvt shim at the end of this
// file forwards here). Identify-first: a profile that decodes and matches
// a registry identity yields that identity (resolved against the caller's
// config when possible); a decodable-but-unmatched profile yields the bare
// "icc:<identifier>" token. There is deliberately NO session-synthetic
// registration on this branch -- nothing consumes a registered synthetic
// yet, so the token itself is the complete answer for the unmatched case.
OIIO::pvt::IccIdentifyResult
identify_icc_profile_impl(const ColorConfig& config, cspan<uint8_t> iccdata)
{
    OIIO::pvt::IccIdentifyResult result;
    if (!OIIO::pvt::is_icc_profile(iccdata))
        return result;  // not ICC: empty id, decodable false
    const std::string token = "icc:"
                              + OIIO::pvt::icc_profile_identifier(iccdata);

    // Decode-validate eagerly: undecodable profiles (cLUT/AToB -- OCIO's
    // reader is matrix/TRC-only) throw when the processor is built.
    OCIO::ConstConfigRcPtr probecfg;
    try {
        probecfg = make_icc_probe_config(iccdata);
        probecfg->getProcessor("icc_probe", "cie_xyz_d65");
    } catch (...) {
        result.id = token;
        return result;  // decodable stays false
    }
    result.decodable = true;

    // Fingerprint the decoded profile against the registry identities. The
    // probe values are authored in CIE-XYZ-D65, which IS this config's
    // display reference, so initialize_probe_values leaves them untouched.
    std::string ciid;
    try {
        auto context       = probecfg->getCurrentContext();
        ProbeValues probes = initialize_probe_values(probecfg, context);
        if (auto fp = compute_fingerprint(probecfg,
                                          probecfg->getColorSpace("icc_probe"),
                                          context, probes))
            ciid = registry_id_for_fingerprint(registry_fingerprint_index(),
                                               *fp);
    } catch (...) {
    }
    if (ciid.empty()) {
        result.id = token;  // decodable, unmatched
        return result;
    }
    // Prefer a caller-local resolution of the matched identity; fall back
    // to the canonical interop id itself.
    string_view local = config.resolve(ciid);
    result.id         = local.empty() ? ciid : std::string(local);
    return result;
}


// ---------------------------------------------------------------------------
// Mastering display volume (SMPTE ST 2086) derivation -- the five-tier
// first-hit-wins ladder, ported from the proven POC:
//   1. ACES-OUTPUT builtin style table (nominal peak + limiting gamut)
//   2. display-interchange CST probe   (view_transform-based views)
//   3. inverse DISPLAY-builtin probe   (v1-style with an encoding tail)
//   4. registry-identity decode probe  (v1-style pure-LUT with interop id)
//   5. no record (honestly yields nothing, never guesses)
// Tiers 2-4 share one numeric probe; they differ only in how the code->XYZ
// decode is built.
// ---------------------------------------------------------------------------

namespace {

// Reference chromaticities for the style table (R, G, B, W xy).
static const float kRec709xy[4][2]  = { { 0.64f, 0.33f },
                                        { 0.30f, 0.60f },
                                        { 0.15f, 0.06f },
                                        { 0.3127f, 0.329f } };
static const float kP3D65xy[4][2]   = { { 0.68f, 0.32f },
                                        { 0.265f, 0.69f },
                                        { 0.15f, 0.06f },
                                        { 0.3127f, 0.329f } };
static const float kRec2020xy[4][2] = { { 0.708f, 0.292f },
                                        { 0.170f, 0.797f },
                                        { 0.131f, 0.046f },
                                        { 0.3127f, 0.329f } };

// Recursive scan for a BuiltinTransform whose style begins with `prefix` in
// a transform tree (GroupTransform children included). ColorSpaceTransform
// indirection is intentionally not followed -- a style reachable only
// through a referenced space is invisible by design; callers fall through
// to their own next tier. `stop_at_first` returns on the first hit (the
// ACES-OUTPUT lookup); otherwise every child is scanned so `style_out`
// holds the LAST match (the DISPLAY-builtin encoding tail, where the tail
// -- not the first hit -- is what's wanted).
bool
find_builtin_style(const OCIO::ConstTransformRcPtr& transform,
                   const char* prefix, bool stop_at_first,
                   std::string& style_out)
{
    if (!transform)
        return false;
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_BUILTIN) {
        auto builtin = OCIO::DynamicPtrCast<const OCIO::BuiltinTransform>(
            transform);
        if (builtin && Strutil::starts_with(builtin->getStyle(), prefix)) {
            style_out = builtin->getStyle();
            return true;
        }
        return false;
    }
    if (transform->getTransformType() == OCIO::TRANSFORM_TYPE_GROUP) {
        auto group = OCIO::DynamicPtrCast<const OCIO::GroupTransform>(
            transform);
        if (!group)
            return false;
        bool found = false;
        for (int i = 0; i < group->getNumTransforms(); ++i) {
            if (find_builtin_style(group->getTransform(i), prefix,
                                   stop_at_first, style_out)) {
                found = true;
                if (stop_at_first)
                    return true;
            }
        }
        return found;
    }
    return false;
}

// DISPLAY-builtin encoding tail prefix of a display encoding style.
static const char* kDisplayBuiltinPrefix = "DISPLAY - CIE-XYZ-D65_to_";

// Parse an ACES-OUTPUT builtin style into a mastering volume. Peak nits is
// the first "<number>nit" token (ACES 1.1 styles carry a second mid-grey
// token -- "1000nit-15nit" -- which is not a mastering value); tokenless
// SDR styles fall back to their ACES-defined nominals (video 100, cinema
// 48). The limiting gamut comes from the P3lim / P3-D65 / REC2020[lim] /
// REC709[lim] token; whitepoint is D65, carried by the tables themselves.
// SDR-VIDEO without a gamut token is Rec.709 by definition. Styles with no
// resolvable gamut (e.g. DCI-white cinema sims) return false and fall
// through to the numeric probe.
bool
mastering_volume_from_style(const std::string& style,
                            OIIO::pvt::MasteringDisplayVolume& volume)
{
    if (!Strutil::starts_with(style, "ACES-OUTPUT"))
        return false;
    double peak       = 0.0;
    const auto nitpos = style.find("nit");
    if (nitpos != std::string::npos) {
        size_t begin = nitpos;
        while (begin > 0
               && (isdigit(static_cast<unsigned char>(style[begin - 1]))
                   || style[begin - 1] == '.'))
            --begin;
        if (begin == nitpos)
            return false;
        peak = Strutil::stod(style.substr(begin, nitpos - begin));
    } else if (style.find("SDR-VIDEO") != std::string::npos) {
        peak = 100.0;
    } else if (style.find("SDR-CINEMA") != std::string::npos) {
        peak = 48.0;
    } else {
        return false;
    }
    const float(*gamut)[2] = nullptr;
    if (style.find("P3lim") != std::string::npos
        || style.find("P3-D65") != std::string::npos)
        gamut = kP3D65xy;
    else if (style.find("REC2020") != std::string::npos)
        gamut = kRec2020xy;
    else if (style.find("REC709") != std::string::npos
             || style.find("SDR-VIDEO") != std::string::npos)
        gamut = kRec709xy;
    if (!gamut)
        return false;
    memcpy(volume.primaries, gamut, sizeof(volume.primaries));
    volume.max_luminance = peak;
    // min stays 0.0 (what a code-0 probe reports for PQ and Rec.1886
    // alike); wire encoders wanting the conventional 0.0001 cd/m^2 floor
    // clamp at encode time.
    volume.min_luminance = 0.0;
    volume.style         = style;
    return true;
}

// Cinema predicate for a "DISPLAY - CIE-XYZ-D65_to_*" builtin style --
// classify by the encoding FAMILY that leads the style suffix, never by
// DCI/DCDM device tokens anywhere in the string. Gamma-2.6 theatrical
// encodings (G2.6-P3-DCI-BFD, G2.6-P3-D60-BFD, G2.6-P3-D65, and DCDM's
// gamma 2.6 with its baked 48/52.37 scale) decode interchange Y relative
// to the 48 cd/m^2 projector calibration white. ST2084/PQ decodes to
// absolute nits/100, and the video encodings (sRGB, G2.2/REC.1886,
// DisplayP3, HLG) are relative to the 100 cd/m^2 video white. Device
// tokens are unreliable: the "DCDM" in ST2084-DCDM-D65 names the XYZ
// container, not a 48-nit convention, and G2.6-P3-D60-BFD carries no
// DCI/DCDM token at all.
bool
is_cinema_display_builtin(const std::string& style)
{
    if (!Strutil::starts_with(style, kDisplayBuiltinPrefix))
        return false;
    string_view suffix = string_view(style).substr(
        strlen(kDisplayBuiltinPrefix));
    return Strutil::starts_with(suffix, "G2.6-P3-")
           || Strutil::starts_with(suffix, "DCDM-");
}

// Cinema classification from an interop identity: the registry twin's
// encoding is the authority (identity-first; the style-suffix predicate
// above is the structural fallback). "sdr-cinema" marks the gamma-2.6
// theatrical encodings (48 cd/m^2 calibration white); "hdr-cinema" marks
// PQ cinema masters, which decode to absolute nits/100 like all PQ and so
// anchor at 100. Linear theatrical spaces stay "display-linear" in the
// registry, so the p3dci gamut token still decides those. Returns -1 when
// the identity has no registry twin -- the caller falls back to whatever
// structural evidence it holds; else 0/1.
int
cinema_from_identity(string_view interop_id)
{
    if (interop_id.empty())
        return -1;
    const RegistryFingerprintIndex& index = registry_fingerprint_index();
    if (!index.config)
        return -1;
    OCIO::ConstColorSpaceRcPtr twin;
    try {
        twin = index.config->getColorSpace(std::string(interop_id).c_str());
    } catch (...) {
        return -1;
    }
    if (!twin)
        return -1;
    const char* enc = twin->getEncoding();
    string_view encoding(enc ? enc : "");
    if (Strutil::iequals(encoding, "sdr-cinema"))
        return 1;
    if (Strutil::iequals(encoding, "display-linear")
        && Strutil::icontains(interop_id, "p3dci"))
        return 1;  // e.g. lin_p3dci_display: linear feed to a 48-nit projector
    return 0;      // known twin with a video or PQ (absolute) encoding
}

// Snap a probed peak to the nearest nominal mastering target. mDCV wants
// the nominal (an ACES 1.1 1000-nit tonescale saturates at ~991.48 through
// the probe), so anything within the 2% window snaps; first match wins.
double
snap_nominal_nits(double nits)
{
    static const double kNominal[] = { 48,  100,  108,  203,  300,  500,
                                       600, 1000, 2000, 4000, 10000 };
    for (double nominal : kNominal)
        if (std::abs(nits - nominal) / nominal < 0.02)
            return nominal;
    return nits;
}

}  // namespace

// Core of pvt::derive_mastering_volume() (the pvt shim at the end of this
// file forwards here).
bool
derive_mastering_volume_impl(const ColorConfig& config, string_view display,
                             string_view view,
                             OIIO::pvt::MasteringDisplayVolume& volume)
{
    auto* impl = pvt::ColorConfigClassificationPeek::impl(config);
    if (!impl || !impl->config_)
        return false;
    const OCIO::ConstConfigRcPtr cfg = impl->config_;

    std::string disp(display);
    std::string vw(view);
    try {
        if (disp.empty())
            disp = cfg->getDefaultDisplay();
        if (disp.empty())
            return false;
        if (vw.empty())
            vw = cfg->getDefaultView(disp.c_str());
        if (vw.empty())
            return false;

        std::string style;                  // ACES-OUTPUT style, if any
        std::string display_encoding_tail;  // v1-style DISPLAY builtin tail
        std::string output_space;           // v1-style output space name
        const char* vtname  = cfg->getDisplayViewTransformName(disp.c_str(),
                                                               vw.c_str());
        const bool vt_based = vtname && vtname[0];
        if (vt_based) {
            if (auto vt = cfg->getViewTransform(vtname)) {
                if (!find_builtin_style(
                        vt->getTransform(OCIO::VIEWTRANSFORM_DIR_FROM_REFERENCE),
                        "ACES-OUTPUT", true, style))
                    find_builtin_style(
                        vt->getTransform(OCIO::VIEWTRANSFORM_DIR_TO_REFERENCE),
                        "ACES-OUTPUT", true, style);
            }
        } else {
            const char* csname = cfg->getDisplayViewColorSpaceName(disp.c_str(),
                                                                   vw.c_str());
            if (csname && csname[0]) {
                output_space = csname;
                if (auto cs = cfg->getColorSpace(csname)) {
                    auto fromref = cs->getTransform(
                        OCIO::COLORSPACE_DIR_FROM_REFERENCE);
                    auto toref = cs->getTransform(
                        OCIO::COLORSPACE_DIR_TO_REFERENCE);
                    if (!find_builtin_style(fromref, "ACES-OUTPUT", true, style))
                        find_builtin_style(toref, "ACES-OUTPUT", true, style);
                    find_builtin_style(fromref, kDisplayBuiltinPrefix, false,
                                       display_encoding_tail);
                    if (display_encoding_tail.empty())
                        find_builtin_style(toref, kDisplayBuiltinPrefix, false,
                                           display_encoding_tail);
                }
            }
        }

        // Tier 1: the style table carries the NOMINAL peak (1000 where the
        // probe sees the tonescale asymptote 991.48) and the LIMITING gamut
        // (the probe can only see the encoding gamut).
        if (mastering_volume_from_style(style, volume))
            return true;

        // Tiers 2-4: build the code->XYZ decode, then run the shared
        // numeric probe. Cinema anchoring (48 vs 100 cd/m^2) is decided per
        // construction, from the evidence each has -- identity-first via
        // the registry twin's encoding, structural style-family fallback.
        OCIO::ConstCPUProcessorRcPtr decode;
        bool cinema  = false;
        auto context = cfg->getCurrentContext();
        if (vt_based) {
            // Tier 2: CST from the display colorspace to the display
            // interchange role.
            const char* dcsname
                = cfg->getDisplayViewColorSpaceName(disp.c_str(), vw.c_str());
            if (!dcsname || !dcsname[0])
                return false;
            std::string tail;
            if (auto dcs = cfg->getColorSpace(dcsname)) {
                find_builtin_style(dcs->getTransform(
                                       OCIO::COLORSPACE_DIR_FROM_REFERENCE),
                                   kDisplayBuiltinPrefix, false, tail);
                if (tail.empty())
                    find_builtin_style(dcs->getTransform(
                                           OCIO::COLORSPACE_DIR_TO_REFERENCE),
                                       kDisplayBuiltinPrefix, false, tail);
            }
            const int idcinema = cinema_from_identity(
                derive_color_interop_id_impl(config, dcsname));
            cinema   = idcinema >= 0 ? (idcinema > 0)
                                     : is_cinema_display_builtin(tail);
            auto cst = OCIO::ColorSpaceTransform::Create();
            cst->setSrc(dcsname);
            cst->setDst(OCIO::ROLE_INTERCHANGE_DISPLAY);
            decode = cfg->getProcessor(context, cst,
                                       OCIO::TRANSFORM_DIR_FORWARD)
                         ->getDefaultCPUProcessor();
            // Provenance: the unparseable ACES style tier 1 found, if any.
        } else if (!display_encoding_tail.empty()) {
            // Tier 3: the LAST DISPLAY builtin in the output space's chain,
            // instantiated INVERSE. A CST is NOT used here -- the v1 output
            // space is scene-referred and a CST to the interchange would
            // re-apply a view transform.
            auto builtin = OCIO::BuiltinTransform::Create();
            builtin->setStyle(display_encoding_tail.c_str());
            builtin->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
            decode = cfg->getProcessor(context, builtin,
                                       OCIO::TRANSFORM_DIR_FORWARD)
                         ->getDefaultCPUProcessor();
            style  = display_encoding_tail;  // provenance: the encoding tail
            cinema = is_cinema_display_builtin(display_encoding_tail);
        } else {
            // Tier 4: registry-identity decode. The identity must be
            // display-referred -- a scene-referred identity cannot anchor
            // display luminance.
            if (output_space.empty())
                return false;
            string_view interop_id = derive_color_interop_id_impl(config,
                                                                  output_space);
            const RegistryFingerprintIndex& index = registry_fingerprint_index();
            if (interop_id.empty() || !index.config)
                return false;
            auto registrycs = index.config->getColorSpace(
                std::string(interop_id).c_str());
            if (!registrycs
                || registrycs->getReferenceSpaceType()
                       != OCIO::REFERENCE_SPACE_DISPLAY)
                return false;
            decode = index.config
                         ->getProcessor(registrycs->getName(),
                                        OCIO::ROLE_INTERCHANGE_DISPLAY)
                         ->getDefaultCPUProcessor();
            style  = interop_id;  // provenance: the identity
            cinema = cinema_from_identity(interop_id) > 0;
        }

        // The shared probe. Luminance: achromatic 1e5 drive through the
        // view (saturates any tonescale), decoded at CIE-XYZ-D65 where
        // Y = nits/anchor; the anchor is 100 (video, PQ absolute) or 48
        // (gamma-2.6 theatrical projector calibration white). Peak is
        // capped at the 10000 PQ container ceiling and snapped to the
        // nominal targets; black is probe-honest (no 0.0001 floor).
        auto dvt = OCIO::DisplayViewTransform::Create();
        dvt->setSrc(OCIO::ROLE_SCENE_LINEAR);
        dvt->setDisplay(disp.c_str());
        dvt->setView(vw.c_str());
        auto dvtcpu = cfg->getProcessor(context, dvt,
                                        OCIO::TRANSFORM_DIR_FORWARD)
                          ->getDefaultCPUProcessor();

        float peakrgb[3] = { 1e5f, 1e5f, 1e5f };
        dvtcpu->applyRGB(peakrgb);
        decode->applyRGB(peakrgb);
        float blackrgb[3] = { 0.0f, 0.0f, 0.0f };
        dvtcpu->applyRGB(blackrgb);
        decode->applyRGB(blackrgb);
        const double anchor  = cinema ? 48.0 : 100.0;
        volume.max_luminance = snap_nominal_nits(
            std::min(double(peakrgb[1]) * anchor, 10000.0));
        volume.min_luminance = std::max(double(blackrgb[1]) * anchor, 0.0);

        // Primaries: decode the four basis vectors R,G,B,W to XYZ and
        // convert to xy. Invariant to the per-channel TRC (a basis vector's
        // xy is independent of the curve), so exact for matrix+TRC
        // encodings; reports the ENCODING gamut (hull-fitting a custom
        // view's true limiting gamut is a known follow-up).
        static const float kBasis[4][3] = { { 1.0f, 0.0f, 0.0f },
                                            { 0.0f, 1.0f, 0.0f },
                                            { 0.0f, 0.0f, 1.0f },
                                            { 1.0f, 1.0f, 1.0f } };
        for (int i = 0; i < 4; ++i) {
            float xyz[3] = { kBasis[i][0], kBasis[i][1], kBasis[i][2] };
            decode->applyRGB(xyz);
            const double sum = double(xyz[0]) + double(xyz[1]) + double(xyz[2]);
            volume.primaries[i][0] = sum != 0.0 ? float(xyz[0] / sum) : 0.0f;
            volume.primaries[i][1] = sum != 0.0 ? float(xyz[1] / sum) : 0.0f;
        }
        volume.style = style;  // provenance: builtin style or interop id
        return true;
    } catch (...) {
        // No display interchange role, unresolvable scene source, etc. --
        // the volume is not derivable from this config.
        return false;
    }
}

OIIO_NAMESPACE_END



// The pvt shims below are declared (OIIO_API) in the library's "current"
// namespace by color_pvt.h, so they must be defined there too, not inside
// the ABI-versioned v3_1 namespace the helpers above live in.
OIIO_NAMESPACE_BEGIN

namespace pvt {


IccIdentifyResult
identify_icc_profile(const ColorConfig& config, cspan<uint8_t> iccdata)
{
    return v3_1::identify_icc_profile_impl(config, iccdata);
}

bool
derive_mastering_volume(const ColorConfig& config, string_view display,
                        string_view view, MasteringDisplayVolume& volume)
{
    return v3_1::derive_mastering_volume_impl(config, display, view, volume);
}

}  // namespace pvt

OIIO_NAMESPACE_END
