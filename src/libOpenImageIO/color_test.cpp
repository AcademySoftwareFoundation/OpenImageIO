// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/benchmark.h>
#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/simd.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/unittest.h>


using namespace OIIO;
using namespace simd;


// Aid for things that are too short to benchmark accurately
#define REP10(x) x, x, x, x, x, x, x, x, x, x

static int iterations = 1000000;
static int ntrials    = 5;
static bool verbose   = false;



static void
getargs(int argc, char* argv[])
{
    ArgParse ap;
    // clang-format off
    ap.intro("color_test\n" OIIO_INTRO_STRING)
      .usage("color_test [options]");

    ap.arg("-v", &verbose)
      .help("Verbose mode");
    ap.arg("--iters %d", &iterations)
      .help(Strutil::fmt::format("Number of iterations (default: {})", iterations));
    ap.arg("--trials %d", &ntrials)
      .help("Number of trials");
    // clang-format on

    ap.parse(argc, (const char**)argv);
}



// Loading a color config is logged as "ColorConfig::init".
static bool
color_config_loaded()
{
    return Strutil::contains(OIIO::get_string_attribute("timing_report"),
                             "ColorConfig");
}



// Must run before anything else loads a color config.
static void
test_lazy_config_loading()
{
    OIIO::attribute("log_times", 1);
    OIIO_CHECK_ASSERT(!color_config_loaded());

    // Setting a color space clears contradicting metadata without a config.
    ImageSpec spec;
    spec.attribute("tiff:ColorSpace", 1);
    spec.attribute("oiio:Gamma", 2.2f);
    spec.set_colorspace("lin_rec709_scene");
    OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                     "lin_rec709_scene");
    OIIO_CHECK_ASSERT(!spec.find_attribute("tiff:ColorSpace"));
    OIIO_CHECK_ASSERT(!spec.find_attribute("oiio:Gamma"));
    set_colorspace_rec709_gamma(spec, 2.2f);
    OIIO_CHECK_EQUAL(spec.get_string_attribute("oiio:ColorSpace"),
                     "g22_rec709_scene");
    OIIO_CHECK_EQUAL(spec.get_float_attribute("oiio:Gamma"), 2.2f);
    // An sRGB name keeps "Exif:ColorSpace" without asking a config.
    spec.attribute("Exif:ColorSpace", 1);
    set_colorspace(spec, "srgb_rec709_scene");
    OIIO_CHECK_ASSERT(spec.find_attribute("Exif:ColorSpace"));

    // Constructing a config and mapping CICP to an interop ID load nothing.
    const ColorConfig& config(ColorConfig::default_colorconfig());
    const int cicp[4] = { 9, 16, 9, 1 };
    OIIO_CHECK_EQUAL(config.get_color_interop_id(cicp), "pq_rec2020_display");
    OIIO_CHECK_ASSERT(!color_config_loaded());

    // Only a non-sRGB name with "Exif:ColorSpace" present asks the config
    // whether to erase it.
    set_colorspace(spec, "lin_rec709_scene");
    OIIO_CHECK_ASSERT(!spec.find_attribute("Exif:ColorSpace"));
    OIIO_CHECK_ASSERT(color_config_loaded());
    OIIO::attribute("log_times", 0);

    // A config that fails to load still says so when first asked.
    ColorConfig missing("no_such_config.ocio");
    OIIO_CHECK_ASSERT(missing.has_error());
}



static void
test_sRGB_conversion()
{
    Benchmarker bench;

    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_sRGB(0.5f), 0.735356983052449f, 1.0e-6);

    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(sRGB_to_linear(0.5f), 0.214041140482232f, 1.0e-6);

    // Check the SIMD versions, too
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(0.0f)), vfloat4(0.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(1.0f)), vfloat4(1.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(linear_to_sRGB(vfloat4(0.5f)),
                                 vfloat4(0.735356983052449f), 1.0e-5);

    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(0.0f)), vfloat4(0.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(1.0f)), vfloat4(1.0f),
                                 1.0e-5);
    OIIO_CHECK_SIMD_EQUAL_THRESH(sRGB_to_linear(vfloat4(0.5f)),
                                 vfloat4(0.214041140482232f), 1.0e-5);

    float fval = 0.5f;
    clobber(fval);
    vfloat4 vfval(fval);
    clobber(vfval);
    bench("sRGB_to_linear",
          [&]() { return DoNotOptimize(sRGB_to_linear(fval)); });
    bench("linear_to_sRGB",
          [&]() { return DoNotOptimize(sRGB_to_linear(fval)); });
    bench.work(4);
    bench("sRGB_to_linear simd",
          [&]() { return DoNotOptimize(sRGB_to_linear(vfval)); });
    bench("linear_to_sRGB simd",
          [&]() { return DoNotOptimize(sRGB_to_linear(vfval)); });
}



static void
test_isData()
{
    // A data space answers by its name, an alias or a role, in any case,
    // whether or not the config's "data" role names it.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {default: linear, scene_linear: linear, data: Raw}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: linear}\n"
        "  - !<ColorSpace> {name: Raw, aliases: [\"Utility - Raw\"], "
        "isdata: true}\n"
        "  - !<ColorSpace> {name: Normals, isdata: true}\n"));
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        for (auto name :
             { "Raw", "RAW", "Utility - Raw", "data", "Normals", "normals" })
            OIIO_CHECK_ASSERT(config.isData(name));
        for (auto name : { "linear", "default", "missing" })
            OIIO_CHECK_FALSE(config.isData(name));
    }
    Filesystem::remove(filename);
}



static void
test_namespaced_ids()
{
    // Namespaced Color Interop IDs fall back as the Color Interop Forum
    // recommends: one namespace is dropped, then "<config>:local:<base>"
    // matches this config's sanitized names.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "name: Show 1 Config\n"
        "roles: {default: ACEScg, scene_linear: ACEScg, data: Raw}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: ACEScg, aliases: [lin_ap1_scene]}\n"
        "  - !<ColorSpace> {name: Utility - sRGB - Texture}\n"
        "  - !<ColorSpace> {name: Raw, isdata: true}\n"));
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(config.resolve("acme:lin_ap1_scene"), "ACEScg");
        OIIO_CHECK_EQUAL(config.resolve("acme:acescg"), "ACEScg");
        OIIO_CHECK_EQUAL(config.resolve(
                             "show_1_config:local:utility_-_srgb_-_texture"),
                         "Utility - sRGB - Texture");
        OIIO_CHECK_ASSERT(config.isData("acme:raw"));
        // Only one namespace is dropped, a role is not a color space name,
        // an empty inner namespace keeps its separator, a local ID names
        // only its own config, and uppercase is not an ID.
        for (auto name :
             { "a:b:lin_ap1_scene", "acme:scene_linear",
               "my-studio::lin_ap1_scene",
               "other:local:utility_-_srgb_-_texture", "Acme:lin_ap1_scene" })
            OIIO_CHECK_EQUAL(config.resolve(name), name);
    }
    Filesystem::remove(filename);
}



static void
test_Rec709_conversion()
{
    Benchmarker bench;

    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(linear_to_Rec709(0.5f), 0.705515089922121f, 1.0e-6);

    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(0.0f), 0.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(1.0f), 1.0f, 1.0e-6);
    OIIO_CHECK_EQUAL_THRESH(Rec709_to_linear(0.5f), 0.259589400506286f, 1.0e-6);

    float fval = 0.5f;
    clobber(fval);
    bench("Rec709_to_linear",
          [&]() { return DoNotOptimize(Rec709_to_linear(fval)); });
    bench("linear_to_Rec709",
          [&]() { return DoNotOptimize(Rec709_to_linear(fval)); });
}



static void
test_linear_display_cicp()
{
    // The linear display identities carry their scene identities' codes,
    // which still read back as the scene identities.
    const ColorConfig& config(ColorConfig::default_colorconfig());
    const struct {
        const char* display;
        const char* scene;
        int cicp[4];
    } cases[] = {
        { "lin_rec709_display", "lin_rec709_scene", { 1, 8, 1, 1 } },
        { "lin_p3d65_display", "lin_p3d65_scene", { 12, 8, 1, 1 } },
        { "lin_rec2020_display", "lin_rec2020_scene", { 9, 8, 10, 1 } },
    };
    for (const auto& c : cases) {
        cspan<int> cicp = config.get_cicp(c.display);
        OIIO_CHECK_ASSERT(cicp == cspan<int>(c.cicp));
        OIIO_CHECK_EQUAL(config.get_color_interop_id(c.cicp), c.scene);
    }
}



static void
test_interop_identities_conversion()
{
    // A color interop ID the config doesn't define converts through OIIO's
    // built-in identities config and this config's interchange roles.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    auto write_config          = [&](string_view aliases) {
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(
            filename,
            Strutil::fmt::format(
                "ocio_profile_version: 2.3\n"
                "roles: {{aces_interchange: ACES2065-1, "
                "cie_xyz_d65_interchange: CIE-XYZ-D65, default: ACES2065-1}}\n"
                "file_rules:\n  - !<Rule> {{name: Default, colorspace: default}}\n"
                "displays:\n  XYZ:\n    - !<View> {{name: Bridge, "
                "view_transform: bridge, display_colorspace: CIE-XYZ-D65}}\n"
                "    - !<View> {{name: Raw, colorspace: Raw}}\n"
                "view_transforms:\n  - !<ViewTransform> {{name: bridge, "
                "from_scene_reference: !<BuiltinTransform> "
                "{{style: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD}}}}\n"
                "colorspaces:\n"
                "  - !<ColorSpace> {{name: ACES2065-1, aliases: [{}]}}\n"
                "  - !<ColorSpace> {{name: Raw, isdata: true}}\n"
                "display_colorspaces:\n"
                "  - !<ColorSpace> {{name: CIE-XYZ-D65}}\n",
                aliases)));
    };
    // Linear Rec.709 red in ACES2065-1 and in CIE XYZ.
    const float rec709_red_ap0[3] = { 0.439633f, 0.0897764f, 0.0175412f };
    const float rec709_red_xyz[3] = { 0.412391f, 0.212639f, 0.0193308f };

    write_config("");
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        auto to_ap0 = config.createColorProcessor("lin_rec709_scene",
                                                  "ACES2065-1");
        auto back   = config.createColorProcessor("ACES2065-1",
                                                  "lin_rec709_scene");
        OIIO_CHECK_ASSERT(to_ap0 && back);
        OIIO_CHECK_ASSERT(
            config.createColorProcessor("ocio:lin_awg4_scene", "ACES2065-1"));
        if (to_ap0 && back) {
            float rgb[3] = { 1.0f, 0.0f, 0.0f };
            to_ap0->apply(rgb);
            for (int c = 0; c < 3; ++c)
                OIIO_CHECK_EQUAL_THRESH(rgb[c], rec709_red_ap0[c], 1.0e-4f);
            back->apply(rgb);
            OIIO_CHECK_EQUAL_THRESH(rgb[0], 1.0f, 1.0e-4f);
            OIIO_CHECK_EQUAL_THRESH(rgb[1], 0.0f, 1.0e-4f);
        }
        // The display pipeline runs from the config's interchange space,
        // which also picks the default view.
        const char* view = config.getDefaultViewName("XYZ", "lin_rec709_scene");
        OIIO_CHECK_EQUAL(string_view(view), "Bridge");
        for (string_view v : { "Bridge", "" }) {
            auto display = config.createDisplayTransform("XYZ", v,
                                                         "lin_rec709_scene",
                                                         "");
            OIIO_CHECK_ASSERT(display);
            if (display) {
                float rgb[3] = { 1.0f, 0.0f, 0.0f };
                display->apply(rgb);
                for (int c = 0; c < 3; ++c)
                    OIIO_CHECK_EQUAL_THRESH(rgb[c], rec709_red_xyz[c], 1.0e-4f);
            }
        }
        // A data view leaves the pixels alone.
        auto raw = config.createDisplayTransform("XYZ", "Raw",
                                                 "lin_rec709_scene", "");
        OIIO_CHECK_ASSERT(raw);
        if (raw) {
            float rgb[3] = { 0.25f, 0.5f, 0.75f };
            raw->apply(rgb);
            OIIO_CHECK_EQUAL(rgb[0], 0.25f);
            OIIO_CHECK_EQUAL(rgb[1], 0.5f);
            OIIO_CHECK_EQUAL(rgb[2], 0.75f);
        }
    }

    // A name the config defines keeps its meaning, even when it misnames
    // the space.
    write_config("lin_rec709_scene");
    {
        ColorConfig config(filename);
        auto proc = config.createColorProcessor("lin_rec709_scene",
                                                "ACES2065-1");
        OIIO_CHECK_ASSERT(proc && proc->isNoOp());
    }
    Filesystem::remove(filename);
}



static void
test_gamma_pair_conversion()
{
    // OCIO < 2.5 composes back-to-back exponents in the wrong direction under
    // its default optimization, giving v^(1.8/2.2) instead of v^(2.2/1.8).
    ColorConfig config("ocio://cg-config-v2.1.0_aces-v1.3_ocio-v2.3");
    auto proc = config.createColorProcessor("Gamma 2.2 Rec.709 - Texture",
                                            "Gamma 1.8 Rec.709 - Texture");
    OIIO_CHECK_ASSERT(proc);
    if (!proc)
        return;
    for (float v : { 0.001f, 0.18f, 0.5f }) {
        float rgb[3] = { v, v, v };
        proc->apply(rgb);
        OIIO_CHECK_EQUAL_THRESH(rgb[0], std::pow(v, 2.2f / 1.8f), 1.0e-4f);
    }
}



static void
test_color_space_properties()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    // Published IDs declared by name or alias, a linear encoding with no ID,
    // one contradicting its ID, and a data space.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {default: ACEScg, scene_linear: ACEScg}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace>\n    name: ACEScg\n    encoding: scene-linear\n"
        "    aliases: [lin_ap1_scene]\n"
        "  - !<ColorSpace> {name: srgb_rec709_scene, encoding: sdr-video}\n"
        "  - !<ColorSpace> {name: Adobe, aliases: [g22_adobergb_scene]}\n"
        "  - !<ColorSpace> {name: Plain, encoding: scene-linear}\n"
        "  - !<ColorSpace>\n    name: Mislabeled\n    encoding: scene-linear\n"
        "    aliases: [g22_rec709_scene]\n"
        "  - !<ColorSpace> {name: Data, isdata: true}\n"
        "  - !<ColorSpace> {name: Texture, aliases: [srgb_texture]}\n"));
    const float ap1[] = { .713f, .293f, .165f,   .83f,
                          .128f, .044f, .32168f, .33767f };
    const float p3d65[] = { .68f, .32f, .265f, .69f, .15f, .06f, .3127f, .329f };
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_ASSERT(config.get_chromaticities("scene_linear")
                          == cspan<float>(ap1));
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("scene_linear"),
                         1.0f);
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("srgb_rec709_scene"),
                         0.0f);
        OIIO_CHECK_EQUAL(config.get_chromaticities("srgb_rec709_scene").size(),
                         8);
        // OIIO's built-in names resolve as they do in the other queries.
        OIIO_CHECK_EQUAL(config.get_chromaticities("sRGB").size(), 8);
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("Adobe"),
                         563.0f / 256.0f);
        for (auto name : { "Plain", "Mislabeled" })
            OIIO_CHECK_EQUAL(config.get_transfer_function_gamma(name), 1.0f);
        OIIO_CHECK_ASSERT(config.get_chromaticities("Plain").empty());
        // The ID gives the primaries even when the encoding contradicts its
        // transfer function, which the encoding decides.
        const float rec709[] = { .64f, .33f, .30f,   .60f,
                                 .15f, .06f, .3127f, .329f };
        OIIO_CHECK_ASSERT(config.get_chromaticities("Mislabeled")
                          == cspan<float>(rec709));
        for (auto name : { "Data", "data", "missing" }) {
            OIIO_CHECK_EQUAL(config.get_transfer_function_gamma(name), 0.0f);
            OIIO_CHECK_ASSERT(config.get_chromaticities(name).empty());
        }
        // A legacy alias, and IDs this config doesn't define, answer as they
        // do for get_color_interop_id() and get_cicp().
        OIIO_CHECK_EQUAL(config.get_color_interop_id("Texture"),
                         "srgb_rec709_scene");
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("Texture"), 0.0f);
        OIIO_CHECK_EQUAL(config.get_chromaticities("Texture").size(), 8);
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("lin_p3d65_display"),
                         1.0f);
        OIIO_CHECK_ASSERT(config.get_chromaticities("lin_p3d65_display")
                          == cspan<float>(p3d65));
    }
    // OpenColorIO 2.3's built-in CG config gives its spaces only the legacy
    // aliases, which identify them here too.
    {
        ColorConfig config("ocio://cg-config-v2.1.0_aces-v1.3_ocio-v2.3");
        OIIO_CHECK_ASSERT(config.get_chromaticities("ACEScg")
                          == cspan<float>(ap1));
        OIIO_CHECK_EQUAL(config.get_chromaticities("sRGB - Texture").size(), 8);
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("sRGB - Texture"),
                         0.0f);
    }
    Filesystem::remove(filename);

    // OpenColorIO 2.5 reads a declared interop_id, which outranks the name.
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02050000) {
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(
            filename,
            "ocio_profile_version: 2.5\n"
            "roles: {default: g18_rec709_scene}\n"
            "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
            "colorspaces:\n  - !<ColorSpace>\n    name: g18_rec709_scene\n"
            "    interop_id: g22_ap1_scene\n"));
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("g18_rec709_scene"),
                         2.2f);
        OIIO_CHECK_ASSERT(config.get_chromaticities("g18_rec709_scene")
                          == cspan<float>(ap1));
        Filesystem::remove(filename);
    }
}



static void
test_derived_color_space_properties()
{
    if (!ColorConfig::supportsOpenColorIO())
        return;

    // Color spaces with no Color Interop ID, written with OCIO's built-in
    // transforms, report what their transforms establish.
    const std::string filename = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".ocio";
    const std::string cube     = Filesystem::temp_directory_path() + "/"
                                 + Filesystem::unique_path() + ".cube";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        cube, "LUT_3D_SIZE 2\n0.1 0.1 0.1\n1 0 0\n0 1 0\n1 1 0\n"
              "0 0 1\n1 0 1\n0 1 1\n1 1 1\n"));
    auto scene = [](const char* name, const char* transform) {
        return Strutil::fmt::format(
            "  - !<ColorSpace>\n    name: {}\n    to_scene_reference: {}\n",
            name, transform);
    };
    auto display = [](const char* name, const char* builtin,
                      const char* encoding = nullptr) {
        return Strutil::fmt::format(
            "  - !<ColorSpace>\n    name: {}\n{}    from_display_reference: "
            "!<BuiltinTransform> {{style: DISPLAY - CIE-XYZ-D65_to_{}}}\n",
            name,
            encoding ? Strutil::fmt::format("    encoding: {}\n", encoding)
                     : std::string(),
            builtin);
    };
    const char* acescg = "!<BuiltinTransform> {style: ACEScg_to_ACES2065-1}";
    const std::string from_xyz
        = "!<BuiltinTransform> {style: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD, "
          "direction: inverse}";
    // RGB to CIE XYZ of P3-D65, and of the primaries (0.7, 0.3), (0.2, 0.75)
    // and (0.12, 0.05) with a D65 white.
    const std::string p3_matrix
        = "!<MatrixTransform> {matrix: [0.4865709486482162, "
          "0.2656676931690931, 0.1982172852343625, 0, 0.2289745640697488, "
          "0.6917385218365064, 0.0792869140937450, 0, 0, 0.0451133818589026, "
          "1.0439443689009760, 0, 0, 0, 0, 1]}";
    const std::string custom_matrix
        = "!<MatrixTransform> {matrix: [0.6204938816007054, "
          "0.1789769595413957, 0.1509850859095705, 0, 0.2659259492574452, "
          "0.6711635982802339, 0.06291045246232105, 0, 0, "
          "0.04474423988534896, 1.044313510874529, 0, 0, 0, 0, 1]}";
    OIIO_CHECK_ASSERT(Filesystem::write_text_file(
        filename,
        "ocio_profile_version: 2.3\n"
        "roles: {aces_interchange: ACES2065-1, "
        "cie_xyz_d65_interchange: CIE-XYZ-D65, default: ACES2065-1}\n"
        "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
        "colorspaces:\n"
        "  - !<ColorSpace> {name: ACES2065-1}\n"
            + scene("ACEScg", acescg)
            + scene("Power AP1",
                    "!<GroupTransform> {children: [!<ExponentTransform> "
                    "{value: 2.2}, !<BuiltinTransform> {style: "
                    "ACEScg_to_ACES2065-1}]}")
            + scene("Log AP1",
                    "!<BuiltinTransform> {style: ACEScct_to_ACES2065-1}")
            // ACEScc's decoder clamps negatives after its matrix.
            + scene("Clamped Log AP1",
                    "!<BuiltinTransform> {style: ACEScc_to_ACES2065-1}")
            // P3-D65 reaching ACES2065-1 through Bradford adaptation.
            + scene("Plain P3", ("!<GroupTransform> {children: [" + p3_matrix
                                 + ", " + from_xyz + "]}")
                                    .c_str())
            // A camera gamut, named as its maker publishes it.
            + scene("RED Log",
                    "!<BuiltinTransform> {style: RED_LOG3G10-RWG_to_ACES2065-1}")
            // Primaries no published gamut has.
            + scene("Custom Gamut", ("!<GroupTransform> {children: ["
                                     + custom_matrix + ", " + from_xyz + "]}")
                                        .c_str())
            // A grade ahead of ACEScg answers as ACEScg.
            + "  - !<ColorSpace>\n    name: Graded AP1\n"
              "    from_scene_reference: !<GroupTransform> {children: ["
              "!<CDLTransform> {slope: [1.1, 1, 0.9]}, "
              "!<ColorSpaceTransform> {src: ACES2065-1, dst: ACEScg}]}\n"
            // A curve applied after the matrix leaves only ACES2065-1's own.
            + scene("Curve Last",
                    "!<GroupTransform> {children: [!<BuiltinTransform> "
                    "{style: ACEScg_to_ACES2065-1}, !<ExponentTransform> "
                    "{value: 2.2}]}")
            // A declared linear encoding is honored over the transforms.
            + "  - !<ColorSpace>\n    name: Said Linear\n"
              "    encoding: scene-linear\n"
              "    to_scene_reference: !<GroupTransform> {children: ["
              "!<ExponentTransform> {value: 2.2}, !<BuiltinTransform> "
              "{style: ACEScg_to_ACES2065-1}]}\n"
            + scene("Cube",
                    ("!<GroupTransform> {children: [!<FileTransform> "
                     "{src: \""
                     + cube + "\", interpolation: linear}, " + acescg + "]}")
                        .c_str())
            + "display_colorspaces:\n"
              "  - !<ColorSpace> {name: CIE-XYZ-D65}\n"
            + display("Monitor 1886", "REC.1886-REC.709")
            + display("Monitor P3", "DisplayP3")
            + display("Cinema P3-D60", "G2.6-P3-D60-BFD")
            + display("Cinema P3-DCI", "G2.6-P3-DCI-BFD")
            // A scene encoding on a display space is not linear for it.
            + display("Mistagged 1886", "REC.1886-REC.709", "scene-linear")));

    const float ap0[] = { .7347f, .2653f, 0.0f,    1.0f,
                          .0001f, -.077f, .32168f, .33767f };
    const float ap1[] = { .713f, .293f, .165f,   .83f,
                          .128f, .044f, .32168f, .33767f };
    const float rec709[] = { .64f, .33f, .30f, .60f, .15f, .06f, .3127f, .329f };
    const float p3d65[] = { .68f, .32f, .265f, .69f, .15f, .06f, .3127f, .329f };
    const float p3d60[] = { .68f, .32f, .265f,   .69f,
                            .15f, .06f, .32168f, .33767f };
    const float p3dci[] = { .68f, .32f, .265f, .69f, .15f, .06f, .314f, .351f };
    const float rwg[]   = { .780308f, .304253f,  .121595f, 1.493994f,
                            .095612f, -.084589f, .3127f,   .329f };
    const float custom[] = { .7f, .3f, .2f, .75f, .12f, .05f, .3127f, .329f };
    // CIE XYZ taken as RGB, so R = G = B is illuminant E.
    const float xyz[]
        = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f / 3, 1.0f / 3 };
    {
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        struct Expected {
            const char* name;
            const float* xy;
            float gamma;
        };
        for (const Expected& e : {
                 Expected { "ACES2065-1", ap0, 1.0f },
                 Expected { "ACEScg", ap1, 1.0f },
                 Expected { "Power AP1", ap1, 0.0f },
                 Expected { "Log AP1", ap1, 0.0f },
                 Expected { "Clamped Log AP1", ap1, 0.0f },
                 Expected { "Plain P3", p3d65, 1.0f },
                 Expected { "RED Log", rwg, 0.0f },
                 Expected { "Custom Gamut", custom, 1.0f },
                 Expected { "Graded AP1", ap1, 1.0f },
                 Expected { "Curve Last", ap0, 0.0f },
                 Expected { "Said Linear", ap1, 1.0f },
                 Expected { "Cube", ap1, 0.0f },
                 Expected { "CIE-XYZ-D65", xyz, 1.0f },
                 Expected { "Monitor 1886", rec709, 0.0f },
                 Expected { "Monitor P3", p3d65, 0.0f },
                 Expected { "Cinema P3-D60", p3d60, 0.0f },
                 Expected { "Cinema P3-DCI", p3dci, 0.0f },
                 Expected { "Mistagged 1886", rec709, 0.0f },
             }) {
            // Compared as text, so a failure names the color space.
            auto describe = [&](cspan<float> xy, float gamma) {
                return Strutil::fmt::format("{}: [{}] {}", e.name,
                                            Strutil::join(xy, " "), gamma);
            };
            OIIO_CHECK_EQUAL(
                describe(config.get_chromaticities(e.name),
                         config.get_transfer_function_gamma(e.name)),
                describe(e.xy ? cspan<float>(e.xy, 8) : cspan<float>(),
                         e.gamma));
        }

        // With analysis off, only declarations and known names answer, even
        // for a space already analyzed.
        OIIO::attribute("colorconfig:analyze_transforms", 0);
        OIIO_CHECK_ASSERT(config.get_chromaticities("Plain P3").empty());
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("Plain P3"), 0.0f);
        OIIO::attribute("colorconfig:analyze_transforms", 1);
    }
    Filesystem::remove(filename);
    Filesystem::remove(cube);

    // OpenColorIO 2.5 reads a declared interop_id. The ID gives the
    // primaries, from the table or from OpenImageIO's own definition of it,
    // whatever the transforms do; an ID OpenImageIO doesn't know leaves them
    // to the transforms.
    if (ColorConfig::OpenColorIO_version_hex() >= 0x02050000) {
        OIIO_CHECK_ASSERT(Filesystem::write_text_file(
            filename,
            "ocio_profile_version: 2.5\n"
            "roles: {aces_interchange: ACES2065-1, "
            "cie_xyz_d65_interchange: CIE-XYZ-D65, default: ACES2065-1}\n"
            "file_rules:\n  - !<Rule> {name: Default, colorspace: default}\n"
            "colorspaces:\n"
            "  - !<ColorSpace> {name: ACES2065-1}\n"
            "  - !<ColorSpace> {name: Camera, interop_id: ocio:lin_awg3_scene}\n"
            "  - !<ColorSpace> {name: Film, interop_id: ocio:adx10_apd_scene}\n"
            "  - !<ColorSpace>\n    name: Studio Wide\n"
            "    interop_id: acme:lin_wide_scene\n"
            "    to_scene_reference: !<GroupTransform> {children: ["
                + p3_matrix + ", " + from_xyz
                + "]}\n"
                  "display_colorspaces:\n"
                  "  - !<ColorSpace> {name: CIE-XYZ-D65}\n"));
        const float awg3[] = { .684f,  .313f,  .221f,  .848f,
                               .0861f, -.102f, .3127f, .329f };
        ColorConfig config(filename);
        OIIO_CHECK_FALSE(config.has_error());
        OIIO_CHECK_ASSERT(config.get_chromaticities("Camera")
                          == cspan<float>(awg3));
        OIIO_CHECK_EQUAL(config.get_chromaticities("Film").size(), 8);
        OIIO_CHECK_ASSERT(config.get_chromaticities("Studio Wide")
                          == cspan<float>(p3d65));
        OIIO_CHECK_EQUAL(config.get_transfer_function_gamma("Studio Wide"),
                         1.0f);
        Filesystem::remove(filename);
    }
}



int
main(int argc, char* argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif

    getargs(argc, argv);

    test_lazy_config_loading();
    test_sRGB_conversion();
    test_isData();
    test_namespaced_ids();
    test_Rec709_conversion();
    test_linear_display_cicp();
    test_interop_identities_conversion();
    test_gamma_pair_conversion();
    test_color_space_properties();
    test_derived_color_space_properties();

    return unit_test_failures != 0;
}
