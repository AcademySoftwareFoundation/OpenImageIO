// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// GENERATED FILE -- DO NOT EDIT BY HAND.
// Regenerate with: python src/build-scripts/gen_color_interop_ids.py
// Source of truth: src/libOpenImageIO/interop-identities-config.ocio
//
// One `inline constexpr string_view` per canonical Color Interop Forum
// ID declared in OIIO's built-in interop identities registry (see the
// CIF recommendation "An ID for Color Interop",
// https://github.com/AcademySoftwareFoundation/ColorInterop/wiki).
// Values are the canonical ID strings -- pass them anywhere a
// string_view CIID is accepted (e.g. ColorConfig::get_color_interop_id);
// no API signature changes. Raw strings remain first-class for ids this
// finite set cannot enumerate (local/custom/icc/user-namespaced ids;
// see ADR-0017 in the color-interop hub).

#pragma once

#include <OpenImageIO/oiioversion.h>
#include <OpenImageIO/string_view.h>

OIIO_NAMESPACE_BEGIN

namespace ColorInteropIDs {

inline constexpr string_view data                   = "data";
inline constexpr string_view g18_rec709_scene       = "g18_rec709_scene";
inline constexpr string_view g22_adobergb_display   = "g22_adobergb_display";
inline constexpr string_view g22_adobergb_scene     = "g22_adobergb_scene";
inline constexpr string_view g22_ap1_scene          = "g22_ap1_scene";
inline constexpr string_view g22_rec709_display     = "g22_rec709_display";
inline constexpr string_view g22_rec709_scene       = "g22_rec709_scene";
inline constexpr string_view g24_rec709_display     = "g24_rec709_display";
inline constexpr string_view g24_rec709_scene       = "g24_rec709_scene";
inline constexpr string_view g26_p3d65_display      = "g26_p3d65_display";
inline constexpr string_view g26_xyzd65_display     = "g26_xyzd65_display";
inline constexpr string_view hlg_rec2020_display    = "hlg_rec2020_display";
inline constexpr string_view lin_adobergb_scene     = "lin_adobergb_scene";
inline constexpr string_view lin_ap0_scene          = "lin_ap0_scene";
inline constexpr string_view lin_ap1_scene          = "lin_ap1_scene";
inline constexpr string_view lin_ciexyzd65_scene    = "lin_ciexyzd65_scene";
inline constexpr string_view lin_p3d65_display      = "lin_p3d65_display";
inline constexpr string_view lin_p3d65_scene        = "lin_p3d65_scene";
inline constexpr string_view lin_rec2020_display    = "lin_rec2020_display";
inline constexpr string_view lin_rec2020_scene      = "lin_rec2020_scene";
inline constexpr string_view lin_rec709_display     = "lin_rec709_display";
inline constexpr string_view lin_rec709_scene       = "lin_rec709_scene";
inline constexpr string_view ocio_acescc_ap1_scene  = "ocio:acescc_ap1_scene";
inline constexpr string_view ocio_acescct_ap1_scene = "ocio:acescct_ap1_scene";
inline constexpr string_view ocio_adx10_apd_scene   = "ocio:adx10_apd_scene";
inline constexpr string_view ocio_adx16_apd_scene   = "ocio:adx16_apd_scene";
inline constexpr string_view ocio_arrilogc3_awg3_scene
    = "ocio:arrilogc3_awg3_scene";
inline constexpr string_view ocio_arrilogc4_awg4_scene
    = "ocio:arrilogc4_awg4_scene";
inline constexpr string_view ocio_bmdfilm5_wg5_scene = "ocio:bmdfilm5_wg5_scene";
inline constexpr string_view ocio_canonlog2_cgamutd55_scene
    = "ocio:canonlog2_cgamutd55_scene";
inline constexpr string_view ocio_canonlog3_cgamutd55_scene
    = "ocio:canonlog3_cgamutd55_scene";
inline constexpr string_view ocio_davinci_dwg_scene = "ocio:davinci_dwg_scene";
inline constexpr string_view ocio_djilog_dgamut_scene
    = "ocio:djilog_dgamut_scene";
inline constexpr string_view ocio_itu709_rec709_scene
    = "ocio:itu709_rec709_scene";
inline constexpr string_view ocio_lin_applewg_scene = "ocio:lin_applewg_scene";
inline constexpr string_view ocio_lin_awg3_scene    = "ocio:lin_awg3_scene";
inline constexpr string_view ocio_lin_awg4_scene    = "ocio:lin_awg4_scene";
inline constexpr string_view ocio_lin_bmdwg5_scene  = "ocio:lin_bmdwg5_scene";
inline constexpr string_view ocio_lin_cgamutd55_scene
    = "ocio:lin_cgamutd55_scene";
inline constexpr string_view ocio_lin_ciexyzd65_display
    = "ocio:lin_ciexyzd65_display";
inline constexpr string_view ocio_lin_dgamut_scene  = "ocio:lin_dgamut_scene";
inline constexpr string_view ocio_lin_dwg_scene     = "ocio:lin_dwg_scene";
inline constexpr string_view ocio_lin_rwg_scene     = "ocio:lin_rwg_scene";
inline constexpr string_view ocio_lin_sgamut3_scene = "ocio:lin_sgamut3_scene";
inline constexpr string_view ocio_lin_sgamut3cine_scene
    = "ocio:lin_sgamut3cine_scene";
inline constexpr string_view ocio_lin_sgamut3cinevenice_scene
    = "ocio:lin_sgamut3cinevenice_scene";
inline constexpr string_view ocio_lin_sgamut3venice_scene
    = "ocio:lin_sgamut3venice_scene";
inline constexpr string_view ocio_lin_vgamut_scene = "ocio:lin_vgamut_scene";
inline constexpr string_view ocio_redlog3g10_rwg_scene
    = "ocio:redlog3g10_rwg_scene";
inline constexpr string_view ocio_slog3_sgamut3_scene
    = "ocio:slog3_sgamut3_scene";
inline constexpr string_view ocio_slog3_sgamut3cine_scene
    = "ocio:slog3_sgamut3cine_scene";
inline constexpr string_view ocio_slog3_sgamut3cinevenice_scene
    = "ocio:slog3_sgamut3cinevenice_scene";
inline constexpr string_view ocio_slog3_sgamut3venice_scene
    = "ocio:slog3_sgamut3venice_scene";
inline constexpr string_view ocio_vlog_vgamut_scene = "ocio:vlog_vgamut_scene";
inline constexpr string_view oiio_applelog_applewg_scene
    = "oiio:applelog_applewg_scene";
inline constexpr string_view oiio_applelog_rec2020_scene
    = "oiio:applelog_rec2020_scene";
inline constexpr string_view oiio_g22_adobergbd50_display
    = "oiio:g22_adobergbd50_display";
inline constexpr string_view oiio_g22_p3d50_display = "oiio:g22_p3d50_display";
inline constexpr string_view oiio_g22_p3d65_display = "oiio:g22_p3d65_display";
inline constexpr string_view oiio_g24_rec2020_display
    = "oiio:g24_rec2020_display";
inline constexpr string_view oiio_g24_rec601_display = "oiio:g24_rec601_display";
inline constexpr string_view oiio_g24_rec601pal_display
    = "oiio:g24_rec601pal_display";
inline constexpr string_view oiio_g26_p3d60_display = "oiio:g26_p3d60_display";
inline constexpr string_view oiio_g26_p3dci_display = "oiio:g26_p3dci_display";
inline constexpr string_view oiio_lin_egamut2_scene = "oiio:lin_egamut2_scene";
inline constexpr string_view oiio_lin_egamut_scene  = "oiio:lin_egamut_scene";
inline constexpr string_view oiio_lin_p3d60_display = "oiio:lin_p3d60_display";
inline constexpr string_view oiio_lin_p3dci_display = "oiio:lin_p3dci_display";
inline constexpr string_view oiio_lin_prophoto_display
    = "oiio:lin_prophoto_display";
inline constexpr string_view oiio_lin_rec601_display = "oiio:lin_rec601_display";
inline constexpr string_view oiio_lin_rec601pal_display
    = "oiio:lin_rec601pal_display";
inline constexpr string_view oiio_pq_rec709_display = "oiio:pq_rec709_display";
inline constexpr string_view oiio_tlog_egamut2_scene = "oiio:tlog_egamut2_scene";
inline constexpr string_view oiio_tlog_egamut_scene = "oiio:tlog_egamut_scene";
inline constexpr string_view pq_p3d65_display       = "pq_p3d65_display";
inline constexpr string_view pq_rec2020_display     = "pq_rec2020_display";
inline constexpr string_view pq_xyzd65_display      = "pq_xyzd65_display";
inline constexpr string_view srgb_ap1_scene         = "srgb_ap1_scene";
inline constexpr string_view srgb_p3d65_display     = "srgb_p3d65_display";
inline constexpr string_view srgb_p3d65_scene       = "srgb_p3d65_scene";
inline constexpr string_view srgb_rec709_display    = "srgb_rec709_display";
inline constexpr string_view srgb_rec709_scene      = "srgb_rec709_scene";
inline constexpr string_view srgbe_p3d65_display    = "srgbe_p3d65_display";

/// Every constant above, for iteration (e.g. the sync test in
/// color_test.cpp). Kept in sync with the individual constants by
/// construction: both are emitted from the same generator run.
inline constexpr string_view all[] = {
    data,
    g18_rec709_scene,
    g22_adobergb_display,
    g22_adobergb_scene,
    g22_ap1_scene,
    g22_rec709_display,
    g22_rec709_scene,
    g24_rec709_display,
    g24_rec709_scene,
    g26_p3d65_display,
    g26_xyzd65_display,
    hlg_rec2020_display,
    lin_adobergb_scene,
    lin_ap0_scene,
    lin_ap1_scene,
    lin_ciexyzd65_scene,
    lin_p3d65_display,
    lin_p3d65_scene,
    lin_rec2020_display,
    lin_rec2020_scene,
    lin_rec709_display,
    lin_rec709_scene,
    ocio_acescc_ap1_scene,
    ocio_acescct_ap1_scene,
    ocio_adx10_apd_scene,
    ocio_adx16_apd_scene,
    ocio_arrilogc3_awg3_scene,
    ocio_arrilogc4_awg4_scene,
    ocio_bmdfilm5_wg5_scene,
    ocio_canonlog2_cgamutd55_scene,
    ocio_canonlog3_cgamutd55_scene,
    ocio_davinci_dwg_scene,
    ocio_djilog_dgamut_scene,
    ocio_itu709_rec709_scene,
    ocio_lin_applewg_scene,
    ocio_lin_awg3_scene,
    ocio_lin_awg4_scene,
    ocio_lin_bmdwg5_scene,
    ocio_lin_cgamutd55_scene,
    ocio_lin_ciexyzd65_display,
    ocio_lin_dgamut_scene,
    ocio_lin_dwg_scene,
    ocio_lin_rwg_scene,
    ocio_lin_sgamut3_scene,
    ocio_lin_sgamut3cine_scene,
    ocio_lin_sgamut3cinevenice_scene,
    ocio_lin_sgamut3venice_scene,
    ocio_lin_vgamut_scene,
    ocio_redlog3g10_rwg_scene,
    ocio_slog3_sgamut3_scene,
    ocio_slog3_sgamut3cine_scene,
    ocio_slog3_sgamut3cinevenice_scene,
    ocio_slog3_sgamut3venice_scene,
    ocio_vlog_vgamut_scene,
    oiio_applelog_applewg_scene,
    oiio_applelog_rec2020_scene,
    oiio_g22_adobergbd50_display,
    oiio_g22_p3d50_display,
    oiio_g22_p3d65_display,
    oiio_g24_rec2020_display,
    oiio_g24_rec601_display,
    oiio_g24_rec601pal_display,
    oiio_g26_p3d60_display,
    oiio_g26_p3dci_display,
    oiio_lin_egamut2_scene,
    oiio_lin_egamut_scene,
    oiio_lin_p3d60_display,
    oiio_lin_p3dci_display,
    oiio_lin_prophoto_display,
    oiio_lin_rec601_display,
    oiio_lin_rec601pal_display,
    oiio_pq_rec709_display,
    oiio_tlog_egamut2_scene,
    oiio_tlog_egamut_scene,
    pq_p3d65_display,
    pq_rec2020_display,
    pq_xyzd65_display,
    srgb_ap1_scene,
    srgb_p3d65_display,
    srgb_p3d65_scene,
    srgb_rec709_display,
    srgb_rec709_scene,
    srgbe_p3d65_display,
};

}  // namespace ColorInteropIDs

OIIO_NAMESPACE_END
