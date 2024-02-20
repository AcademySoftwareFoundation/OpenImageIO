// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

// clang-format off

// See: https://sno.phy.queensu.ca/~phil/exiftool/TagNames/Canon.html
// 

#include <algorithm>
#include <array>
#include <type_traits>

#include "exif.h"

OIIO_NAMESPACE_BEGIN
namespace pvt {


static LabelIndex canon_macromode_table[] = {
    { 1, "macro" }, { 2, "normal" }, { -1, nullptr }
};

static LabelIndex canon_quality_table[] = {
    { 1, "economy" }, { 2, "normal" }, { 2, "fine" }, { 4, "RAW" },
    { 5, "superfine" }, { 130, "normal movie" }, { 131, "movie(2)" },
    { -1, nullptr }
};

static LabelIndex canon_flashmode_table[] = {
    { 0, "off" }, { 1, "auto" }, { 2, "on" }, { 3, "red-eye reduction" },
    { 4, "slow-sync" }, { 5, "red-eye reduction (auto)" },
    { 6, "red-eye reduction (on)" }, { 16, "external flash" },
    { -1, nullptr }
};

static LabelIndex canon_continuousdrive_table[] = {
    { 0, "single" },
    { 1, "continuous" },
    { 2, "movie" },
    { 3, "continuous, speed priority" },
    { 4, "continuous, low" },
    { 5, "continuous, high" },
    { 6, "silent single" },
    { 9, "single, silent" },
    { 10, "continuous, silent" },
    { -1, nullptr }
};

static LabelIndex canon_focusmode_table[] = {
    { 0, "one-shot AF" }, { 1, "AI servo AF" }, { 2, "AI focus AF" },
    { 3, "manual focus(3)" }, { 4, "single" }, { 5, "continuous" },
    { 6, "manual focus(6)" }, { 16, "pan focus" }, { 256, "AF + MF" },
    { 512, "movie snap focus" }, { 519, "movie servo AF" }, { -1, nullptr }
};

static LabelIndex canon_recordmode_table[] = {
    { 1, "JPEG" }, { 2, "CRW+THM" }, { 3, "AVI+THM" }, { 4, "TIF" },
    { 5, "TIF+JPEG" }, { 6, "CR2" }, { 7, "CR2+JPEG" }, { 9, "MOV" },
    { 10, "MP4" }, { -1, nullptr }
};

static LabelIndex canon_imagesize_table[] = {
    { 0, "large" }, { 1, "medium" }, { 2, "small" }, { 5, "medium 1" },
    { 6, "medium 2" }, { 7, "medium 3" }, { 8, "postcard" },
    { 9, "widescreen" }, { 10, "medium widescreen" }, { 14, "small 1" },
    { 15, "small 2" }, { 16, "small 3" }, { 128, "640x480 movie" },
    { 129, "medium movie" }, { 130, "small movie" }, { 137, "1280x720 movie" },
    { 142, "1920x1080 movie" }, { -1, nullptr }
};

static LabelIndex canon_easymode_table[] = {
    { 0, "Full auto" }, { 1, "Manual" }, { 2, "Landscape" }, { 3, "Fast shutter" },
    { 4, "Slow shutter" }, { 5, "Night" }, { 6, "Gray Scale" }, { 7, "Sepia" },
    { 8, "Portrait" }, { 9, "Sports" }, { 10, "Macro" }, { 11, "Black & White" },
    { 12, "Pan focus" }, { 13, "Vivid" }, { 14, "Neutral" }, { 15, "Flash Off" },
    { 16, "Long Shutter" }, { 17, "Super Macro" }, { 18, "Foliage" },
    { 19, "Indoor" }, { 20, "Fireworks" }, { 21, "Beach" }, { 22, "Underwater" },
    { 23, "Snow" }, { 24, "Kids & Pets" }, { 25, "Night Snapshot" },
    { 26, "Digital Macro" }, { 27, "My Colors" }, { 28, "Movie Snap" },
    { 29, "Super Macro 2" }, { 30, "Color Accent" }, { 31, "Color Swap" },
    { 32, "Aquarium" }, { 33, "ISO 3200" }, { 34, "ISO 6400" },
    { 35, "Creative Light Effect" }, { 36, "Easy" }, { 37, "Quick Shot" },
    { 38, "Creative Auto" }, { 39, "Zoom Blur" }, { 40, "Low Light" },
    { 41, "Nostalgic" }, { 42, "Super Vivid" }, { 43, "Poster Effect" },
    { 44, "Face Self-timer" }, { 45, "Smile" }, { 46, "Wink Self-timer" },
    { 47, "Fisheye Effect" }, { 48, "Miniature Effect" }, { 49, "High-speed Burst" },
    { 50, "Best Image Selection" }, { 51, "High Dynamic Range" },
    { 52, "Handheld Night Scene" }, { 53, "Movie Digest" },
    { 54, "Live View Control" }, { 55, "Discreet" }, { 56, "Blur Reduction" },
    { 57, "Monochrome" }, { 58, "Toy Camera Effect" }, { 59, "Scene Intelligent Auto" },
    { 60, "High-speed Burst HQ" }, { 61, "Smooth Skin" }, { 62, "Soft Focus" },
    { 257, "Spotlight" }, { 258, "Night 2" }, { 259, "Night+" }, { 260, "Super Night" },
    { 261, "Sunset" }, { 263, "Night Scene" }, { 264, "Surface" },
    { 265, "Low Light 2" }, { -1, nullptr }
};

static LabelIndex canon_digitalzoom_table[] = {
    { 0, "none" }, { 1, "2x" }, { 2, "4x" }, { 3, "other" }, { -1, nullptr }
};

static LabelIndex canon_meteringmode_table[] = {
    { 0, "default" }, { 1, "spot" }, { 2, "average" }, { 3, "evaluative" },
    { 4, "partial" }, { 5, "center-weighted average" }, { -1, nullptr }
};

static LabelIndex canon_focusrange_table[] = {
    { 0, "manual" }, { 1, "auto" }, { 2, "not known" }, { 3, "macro" },
    { 4, "very close" }, { 5, "close" }, { 6, "middle range" },
    { 7, "far range" }, { 8, "pan focus" }, { 9, "super macro" },
    { 10, "infinity" }, { -1, nullptr }
};

static LabelIndex canon_afpoint_table[] = {
    { 0x2005, "Manual AF point selection" }, { 0x3000, "None (MF)" },
    { 0x3001, "Auto AF point selection" }, { 0x3002, "Right" },
    { 0x3003, "Center" }, { 0x3004, "Left" },
    { 0x4001, "Auto AF point selection" }, { 0x4006, "Face Detect" },
    { -1, nullptr }
};

static LabelIndex canon_exposuremode_table[] = {
    { 0, "Easy" }, { 1, "Program AE" }, { 2, "Shutter speed priority AE" },
    { 3, "Aperture-priority AE" }, { 4, "Manual" }, { 5, "Depth-of-field AE" },
    { 6, "M-Dep" }, { 7, "Bulb" }, { -1, nullptr }
};

static std::string
explain_canon_flashbits (const ParamValue &p, const void* /*extradata*/)
{
    int val = p.get_int();
    if (val == 0)
        return "none";
    std::vector<std::string> bits;
    if (val & (1<<0))  bits.emplace_back ("manual");
    if (val & (1<<1))  bits.emplace_back ("TTL");
    if (val & (1<<2))  bits.emplace_back ("A-TTL");
    if (val & (1<<3))  bits.emplace_back ("E-TTL");
    if (val & (1<<4))  bits.emplace_back ("FP sync enabled");
    if (val & (1<<7))  bits.emplace_back ("2nd-curtain sync used");
    if (val & (1<<11)) bits.emplace_back ("FP sync used");
    if (val & (1<<13)) bits.emplace_back ("built-in");
    if (val & (1<<14)) bits.emplace_back ("external");
    return Strutil::join (bits, ", ");
}

static LabelIndex canon_focuscontinuous_table[] = {
    { 0, "single" }, { 1, "continuous" }, { 8, "manual" }, { -1, nullptr }
};

static LabelIndex canon_aesetting_table[] = {
    { 0, "normal AE" }, { 1, "exposure compensation" }, { 2, "AE lock" },
    { 3, "AE lock + exposure compensation" }, { 4, "no AE" }, { -1, nullptr }
};

static LabelIndex canon_imagestabilization_table[] = {
    { 0, "Off" }, { 1, "On" }, { 2, "Shoot Only" }, { 3, "Panning" },
    { 4, "Dynamic" }, { 256, "Off (2)" }, { 257, "On (2)" },
    { 258, "Shoot Only (2)" }, { 259, "Panning (2)" }, { 260, "Dynamic (2)" },
    { -1, nullptr }
};

static LabelIndex canon_spotmeteringmode_table[] = {
    { 0, "center" }, { 1, "AF point" }, { -1, nullptr }
};

static LabelIndex canon_photoeffect_table[] = {
    { 0, "off" }, { 1, "vivid" }, { 2, "neutral" }, { 3, "smooth" },
    { 4, "sepia" }, { 5, "b&w" }, { 6, "custom" }, { 100, "my color data" },
    { -1, nullptr }
};

static LabelIndex canon_manualflashoutput_table[] = {
    { 0, "n/a" }, { 0x500, "full" }, { 0x502, "medium" }, { 0x504, "low" },
    { 0x7fff, "n/a" }, { -1, nullptr }
};

static LabelIndex canon_srawquality_table[] = {
    { 0, "n/a" }, { 1, "sRAW1 (mRAW)" }, { 2, "sRAW2 (sRAW)" }, { -1, nullptr }
};

static LabelIndex canon_slowshutter_table[] = {
    { 0, "off" }, { 1, "night scene" }, { 2, "on" }, { 3, "none" }, { -1, nullptr }
};

static LabelIndex canon_afpointsinfocus_table[] = {
    { 0x3000, "none" }, { 0x3001, "right" }, { 0x3002, "center" },
    { 0x3003, "center+right" }, { 0x3004, "left" }, { 0x3005, "left+right" },
    { 0x3006, "left+center" }, { 0x3007, "all" }, { -1, nullptr }
};

static LabelIndex canon_autoexposurebracketing_table[] = {
    { -1, "on" }, { 0, "off" }, { 1, "on shot 1" }, { 2, "on shot 2" },
    { 3, "on shot 3" }, { -1, nullptr }
};

static LabelIndex canon_controlmode_table[] = {
    { 0, "n/a" }, { 1, "camera local control" },
    { 3, "computer remote control" }, { -1, nullptr }
};

static LabelIndex canon_cameratype_table[] = {
    { 0, "n/a" }, { 248, "EOS High-end" }, { 250, "Compact" },
    { 252, "EOS Mid-range" }, { 2554, "DV Camera" }, { -1, nullptr }
};

static LabelIndex canon_autorotate_table[] = {
    { -1, "n/a" }, { 0, "none" }, { 1, "rotate 90 CW" },
    { 2, "rotate 180" }, { 3, "rotate 270 CW" }, { -1, nullptr }
};

static LabelIndex canon_ndfilter_table[] = {
    { -1, "n/a" }, { 0, "off" }, { 1, "on" }, { -1, nullptr }
};

static LabelIndex canon_whitebalance_table[] = {
    { 0,  "Auto" }, { 1,  "Daylight" }, { 2,  "Cloudy" }, { 3,  "Tungsten" },
    { 4,  "Fluorescent" }, { 5,  "Flash" }, { 6,  "Custom" }, { 7,  "Black & White" },
    { 8,  "Shade" }, { 9,  "Manual Temperature (Kelvin)" }, { 10, "PC Set1" },
    { 11, "PC Set2" }, { 12, "PC Set3" }, { 14, "Daylight Fluorescent" },
    { 15, "Custom 1" }, { 16, "Custom 2" }, { 17, "Underwater" },
    { 18, "Custom 3" }, { 19, "Custom 4" }, { 20, "PC Set4" }, { 21, "PC Set5" },
    { 23, "Auto (ambience priority)" }, { -1, nullptr }
};

static LabelIndex canon_modelid_table[] = {
    { int(0x1010000), "PowerShot A30" },
    { int(0x1040000), "PowerShot S300 / Digital IXUS 300 / IXY Digital 300" },
    { int(0x1060000), "PowerShot A20" },
    { int(0x1080000), "PowerShot A10" },
    { int(0x1090000), "PowerShot S110 / Digital IXUS v / IXY Digital 200" },
    { int(0x1100000), "PowerShot G2" },
    { int(0x1110000), "PowerShot S40" },
    { int(0x1120000), "PowerShot S30" },
    { int(0x1130000), "PowerShot A40" },
    { int(0x1140000), "EOS D30" },
    { int(0x1150000), "PowerShot A100" },
    { int(0x1160000), "PowerShot S200 / Digital IXUS v2 / IXY Digital 200a" },
    { int(0x1170000), "PowerShot A200" },
    { int(0x1180000), "PowerShot S330 / Digital IXUS 330 / IXY Digital 300a" },
    { int(0x1190000), "PowerShot G3" },
    { int(0x1210000), "PowerShot S45" },
    { int(0x1230000), "PowerShot SD100 / Digital IXUS II / IXY Digital 30" },
    { int(0x1240000), "PowerShot S230 / Digital IXUS v3 / IXY Digital 320" },
    { int(0x1250000), "PowerShot A70" },
    { int(0x1260000), "PowerShot A60" },
    { int(0x1270000), "PowerShot S400 / Digital IXUS 400 / IXY Digital 400" },
    { int(0x1290000), "PowerShot G5" },
    { int(0x1300000), "PowerShot A300" },
    { int(0x1310000), "PowerShot S50" },
    { int(0x1340000), "PowerShot A80" },
    { int(0x1350000), "PowerShot SD10 / Digital IXUS i / IXY Digital L" },
    { int(0x1360000), "PowerShot S1 IS" },
    { int(0x1370000), "PowerShot Pro1" },
    { int(0x1380000), "PowerShot S70" },
    { int(0x1390000), "PowerShot S60" },
    { int(0x1400000), "PowerShot G6" },
    { int(0x1410000), "PowerShot S500 / Digital IXUS 500 / IXY Digital 500" },
    { int(0x1420000), "PowerShot A75" },
    { int(0x1440000), "PowerShot SD110 / Digital IXUS IIs / IXY Digital 30a" },
    { int(0x1450000), "PowerShot A400" },
    { int(0x1470000), "PowerShot A310" },
    { int(0x1490000), "PowerShot A85" },
    { int(0x1520000), "PowerShot S410 / Digital IXUS 430 / IXY Digital 450" },
    { int(0x1530000), "PowerShot A95" },
    { int(0x1540000), "PowerShot SD300 / Digital IXUS 40 / IXY Digital 50" },
    { int(0x1550000), "PowerShot SD200 / Digital IXUS 30 / IXY Digital 40" },
    { int(0x1560000), "PowerShot A520" },
    { int(0x1570000), "PowerShot A510" },
    { int(0x1590000), "PowerShot SD20 / Digital IXUS i5 / IXY Digital L2" },
    { int(0x1640000), "PowerShot S2 IS" },
    { int(0x1650000), "PowerShot SD430 / Digital IXUS Wireless / IXY Digital Wireless" },
    { int(0x1660000), "PowerShot SD500 / Digital IXUS 700 / IXY Digital 600" },
    { int(0x1668000), "EOS D60" },
    { int(0x1700000), "PowerShot SD30 / Digital IXUS i Zoom / IXY Digital L3" },
    { int(0x1740000), "PowerShot A430" },
    { int(0x1750000), "PowerShot A410" },
    { int(0x1760000), "PowerShot S80" },
    { int(0x1780000), "PowerShot A620" },
    { int(0x1790000), "PowerShot A610" },
    { int(0x1800000), "PowerShot SD630 / Digital IXUS 65 / IXY Digital 80" },
    { int(0x1810000), "PowerShot SD450 / Digital IXUS 55 / IXY Digital 60" },
    { int(0x1820000), "PowerShot TX1" },
    { int(0x1870000), "PowerShot SD400 / Digital IXUS 50 / IXY Digital 55" },
    { int(0x1880000), "PowerShot A420" },
    { int(0x1890000), "PowerShot SD900 / Digital IXUS 900 Ti / IXY Digital 1000" },
    { int(0x1900000), "PowerShot SD550 / Digital IXUS 750 / IXY Digital 700" },
    { int(0x1920000), "PowerShot A700" },
    { int(0x1940000), "PowerShot SD700 IS / Digital IXUS 800 IS / IXY Digital 800 IS" },
    { int(0x1950000), "PowerShot S3 IS" },
    { int(0x1960000), "PowerShot A540" },
    { int(0x1970000), "PowerShot SD600 / Digital IXUS 60 / IXY Digital 70" },
    { int(0x1980000), "PowerShot G7" },
    { int(0x1990000), "PowerShot A530" },
    { int(0x2000000), "PowerShot SD800 IS / Digital IXUS 850 IS / IXY Digital 900 IS" },
    { int(0x2010000), "PowerShot SD40 / Digital IXUS i7 / IXY Digital L4" },
    { int(0x2020000), "PowerShot A710 IS" },
    { int(0x2030000), "PowerShot A640" },
    { int(0x2040000), "PowerShot A630" },
    { int(0x2090000), "PowerShot S5 IS" },
    { int(0x2100000), "PowerShot A460" },
    { int(0x2120000), "PowerShot SD850 IS / Digital IXUS 950 IS / IXY Digital 810 IS" },
    { int(0x2130000), "PowerShot A570 IS" },
    { int(0x2140000), "PowerShot A560" },
    { int(0x2150000), "PowerShot SD750 / Digital IXUS 75 / IXY Digital 90" },
    { int(0x2160000), "PowerShot SD1000 / Digital IXUS 70 / IXY Digital 10" },
    { int(0x2180000), "PowerShot A550" },
    { int(0x2190000), "PowerShot A450" },
    { int(0x2230000), "PowerShot G9" },
    { int(0x2240000), "PowerShot A650 IS" },
    { int(0x2260000), "PowerShot A720 IS" },
    { int(0x2290000), "PowerShot SX100 IS" },
    { int(0x2300000), "PowerShot SD950 IS / Digital IXUS 960 IS / IXY Digital 2000 IS" },
    { int(0x2310000), "PowerShot SD870 IS / Digital IXUS 860 IS / IXY Digital 910 IS" },
    { int(0x2320000), "PowerShot SD890 IS / Digital IXUS 970 IS / IXY Digital 820 IS" },
    { int(0x2360000), "PowerShot SD790 IS / Digital IXUS 90 IS / IXY Digital 95 IS" },
    { int(0x2370000), "PowerShot SD770 IS / Digital IXUS 85 IS / IXY Digital 25 IS" },
    { int(0x2380000), "PowerShot A590 IS" },
    { int(0x2390000), "PowerShot A580" },
    { int(0x2420000), "PowerShot A470" },
    { int(0x2430000), "PowerShot SD1100 IS / Digital IXUS 80 IS / IXY Digital 20 IS" },
    { int(0x2460000), "PowerShot SX1 IS" },
    { int(0x2470000), "PowerShot SX10 IS" },
    { int(0x2480000), "PowerShot A1000 IS" },
    { int(0x2490000), "PowerShot G10" },
    { int(0x2510000), "PowerShot A2000 IS" },
    { int(0x2520000), "PowerShot SX110 IS" },
    { int(0x2530000), "PowerShot SD990 IS / Digital IXUS 980 IS / IXY Digital 3000 IS" },
    { int(0x2540000), "PowerShot SD880 IS / Digital IXUS 870 IS / IXY Digital 920 IS" },
    { int(0x2550000), "PowerShot E1" },
    { int(0x2560000), "PowerShot D10" },
    { int(0x2570000), "PowerShot SD960 IS / Digital IXUS 110 IS / IXY Digital 510 IS" },
    { int(0x2580000), "PowerShot A2100 IS" },
    { int(0x2590000), "PowerShot A480" },
    { int(0x2600000), "PowerShot SX200 IS" },
    { int(0x2610000), "PowerShot SD970 IS / Digital IXUS 990 IS / IXY Digital 830 IS" },
    { int(0x2620000), "PowerShot SD780 IS / Digital IXUS 100 IS / IXY Digital 210 IS" },
    { int(0x2630000), "PowerShot A1100 IS" },
    { int(0x2640000), "PowerShot SD1200 IS / Digital IXUS 95 IS / IXY Digital 110 IS" },
    { int(0x2700000), "PowerShot G11" },
    { int(0x2710000), "PowerShot SX120 IS" },
    { int(0x2720000), "PowerShot S90" },
    { int(0x2750000), "PowerShot SX20 IS" },
    { int(0x2760000), "PowerShot SD980 IS / Digital IXUS 200 IS / IXY Digital 930 IS" },
    { int(0x2770000), "PowerShot SD940 IS / Digital IXUS 120 IS / IXY Digital 220 IS" },
    { int(0x2800000), "PowerShot A495" },
    { int(0x2810000), "PowerShot A490" },
    { int(0x2820000), "PowerShot A3100/A3150 IS" },
    { int(0x2830000), "PowerShot A3000 IS" },
    { int(0x2840000), "PowerShot SD1400 IS / IXUS 130 / IXY 400F" },
    { int(0x2850000), "PowerShot SD1300 IS / IXUS 105 / IXY 200F" },
    { int(0x2860000), "PowerShot SD3500 IS / IXUS 210 / IXY 10S" },
    { int(0x2870000), "PowerShot SX210 IS" },
    { int(0x2880000), "PowerShot SD4000 IS / IXUS 300 HS / IXY 30S" },
    { int(0x2890000), "PowerShot SD4500 IS / IXUS 1000 HS / IXY 50S" },
    { int(0x2920000), "PowerShot G12" },
    { int(0x2930000), "PowerShot SX30 IS" },
    { int(0x2940000), "PowerShot SX130 IS" },
    { int(0x2950000), "PowerShot S95" },
    { int(0x2980000), "PowerShot A3300 IS" },
    { int(0x2990000), "PowerShot A3200 IS" },
    { int(0x3000000), "PowerShot ELPH 500 HS / IXUS 310 HS / IXY 31S" },
    { int(0x3010000), "PowerShot Pro90 IS" },
    { int(0x3010001), "PowerShot A800" },
    { int(0x3020000), "PowerShot ELPH 100 HS / IXUS 115 HS / IXY 210F" },
    { int(0x3030000), "PowerShot SX230 HS" },
    { int(0x3040000), "PowerShot ELPH 300 HS / IXUS 220 HS / IXY 410F" },
    { int(0x3050000), "PowerShot A2200" },
    { int(0x3060000), "PowerShot A1200" },
    { int(0x3070000), "PowerShot SX220 HS" },
    { int(0x3080000), "PowerShot G1 X" },
    { int(0x3090000), "PowerShot SX150 IS" },
    { int(0x3100000), "PowerShot ELPH 510 HS / IXUS 1100 HS / IXY 51S" },
    { int(0x3110000), "PowerShot S100 (new)" },
    { int(0x3120000), "PowerShot ELPH 310 HS / IXUS 230 HS / IXY 600F" },
    { int(0x3130000), "PowerShot SX40 HS" },
    { int(0x3140000), "IXY 32S" },
    { int(0x3160000), "PowerShot A1300" },
    { int(0x3170000), "PowerShot A810" },
    { int(0x3180000), "PowerShot ELPH 320 HS / IXUS 240 HS / IXY 420F" },
    { int(0x3190000), "PowerShot ELPH 110 HS / IXUS 125 HS / IXY 220F" },
    { int(0x3200000), "PowerShot D20" },
    { int(0x3210000), "PowerShot A4000 IS" },
    { int(0x3220000), "PowerShot SX260 HS" },
    { int(0x3230000), "PowerShot SX240 HS" },
    { int(0x3240000), "PowerShot ELPH 530 HS / IXUS 510 HS / IXY 1" },
    { int(0x3250000), "PowerShot ELPH 520 HS / IXUS 500 HS / IXY 3" },
    { int(0x3260000), "PowerShot A3400 IS" },
    { int(0x3270000), "PowerShot A2400 IS" },
    { int(0x3280000), "PowerShot A2300" },
    { int(0x3330000), "PowerShot G15" },
    { int(0x3340000), "PowerShot SX50 HS" },
    { int(0x3350000), "PowerShot SX160 IS" },
    { int(0x3360000), "PowerShot S110 (new)" },
    { int(0x3370000), "PowerShot SX500 IS" },
    { int(0x3380000), "PowerShot N" },
    { int(0x3390000), "IXUS 245 HS / IXY 430F" },
    { int(0x3400000), "PowerShot SX280 HS" },
    { int(0x3410000), "PowerShot SX270 HS" },
    { int(0x3420000), "PowerShot A3500 IS" },
    { int(0x3430000), "PowerShot A2600" },
    { int(0x3440000), "PowerShot SX275 HS" },
    { int(0x3450000), "PowerShot A1400" },
    { int(0x3460000), "PowerShot ELPH 130 IS / IXUS 140 / IXY 110F" },
    { int(0x3470000), "PowerShot ELPH 115/120 IS / IXUS 132/135 / IXY 90F/100F" },
    { int(0x3490000), "PowerShot ELPH 330 HS / IXUS 255 HS / IXY 610F" },
    { int(0x3510000), "PowerShot A2500" },
    { int(0x3540000), "PowerShot G16" },
    { int(0x3550000), "PowerShot S120" },
    { int(0x3560000), "PowerShot SX170 IS" },
    { int(0x3580000), "PowerShot SX510 HS" },
    { int(0x3590000), "PowerShot S200 (new)" },
    { int(0x3600000), "IXY 620F" },
    { int(0x3610000), "PowerShot N100" },
    { int(0x3640000), "PowerShot G1 X Mark II" },
    { int(0x3650000), "PowerShot D30" },
    { int(0x3660000), "PowerShot SX700 HS" },
    { int(0x3670000), "PowerShot SX600 HS" },
    { int(0x3680000), "PowerShot ELPH 140 IS / IXUS 150 / IXY 130" },
    { int(0x3690000), "PowerShot ELPH 135 / IXUS 145 / IXY 120" },
    { int(0x3700000), "PowerShot ELPH 340 HS / IXUS 265 HS / IXY 630" },
    { int(0x3710000), "PowerShot ELPH 150 IS / IXUS 155 / IXY 140" },
    { int(0x3740000), "EOS M3" },
    { int(0x3750000), "PowerShot SX60 HS" },
    { int(0x3760000), "PowerShot SX520 HS" },
    { int(0x3770000), "PowerShot SX400 IS" },
    { int(0x3780000), "PowerShot G7 X" },
    { int(0x3790000), "PowerShot N2" },
    { int(0x3800000), "PowerShot SX530 HS" },
    { int(0x3820000), "PowerShot SX710 HS" },
    { int(0x3830000), "PowerShot SX610 HS" },
    { int(0x3840000), "EOS M10" },
    { int(0x3850000), "PowerShot G3 X" },
    { int(0x3860000), "PowerShot ELPH 165 HS / IXUS 165 / IXY 160" },
    { int(0x3870000), "PowerShot ELPH 160 / IXUS 160" },
    { int(0x3880000), "PowerShot ELPH 350 HS / IXUS 275 HS / IXY 640" },
    { int(0x3890000), "PowerShot ELPH 170 IS / IXUS 170" },
    { int(0x3910000), "PowerShot SX410 IS" },
    { int(0x3930000), "PowerShot G9 X" },
    { int(0x3940000), "EOS M5" },
    { int(0x3950000), "PowerShot G5 X" },
    { int(0x3970000), "PowerShot G7 X Mark II" },
    { int(0x3990000), "PowerShot ELPH 360 HS / IXUS 285 HS / IXY 650" },
    { int(0x4010000), "PowerShot SX540 HS" },
    { int(0x4020000), "PowerShot SX420 IS" },
    { int(0x4030000), "PowerShot ELPH 190 IS / IXUS 180 / IXY 190" },
    { int(0x4040000), "PowerShot G1" },
    { int(0x4040001), "IXY 180" },
    { int(0x4050000), "PowerShot SX720 HS" },
    { int(0x4060000), "PowerShot SX620 HS" },
    { int(0x4070000), "EOS M6" },
    { int(0x4100000), "PowerShot G9 X Mark II" },
    { int(0x4150000), "PowerShot ELPH 185 / IXUS 185 / IXY 200" },
    { int(0x4160000), "PowerShot SX430 IS" },
    { int(0x4170000), "PowerShot SX730 HS" },
    { int(0x6040000), "PowerShot S100 / Digital IXUS / IXY Digital" },
    { int(0x4007d673), "C19/DC21/DC22" },
    { int(0x4007d674), "H A1" },
    { int(0x4007d675), "V10" },
    { int(0x4007d676), "D130/MD140/MD150/MD160/ZR850" },
    { int(0x4007d777), "C50" },
    { int(0x4007d778), "V20" },
    { int(0x4007d779), "C211" },
    { int(0x4007d77a), "G10" },
    { int(0x4007d77b), "R10" },
    { int(0x4007d77d), "D255/ZR950" },
    { int(0x4007d81c), "F11" },
    { int(0x4007d878), "V30" },
    { int(0x4007d87c), "H A1S" },
    { int(0x4007d87e), "C301/DC310/DC311/DC320/DC330" },
    { int(0x4007d87f), "S100" },
    { int(0x4007d880), "F10" },
    { int(0x4007d882), "G20/HG21" },
    { int(0x4007d925), "F21" },
    { int(0x4007d926), "F S11" },
    { int(0x4007d978), "V40" },
    { int(0x4007d987), "C410/DC411/DC420" },
    { int(0x4007d988), "S19/FS20/FS21/FS22/FS200" },
    { int(0x4007d989), "F20/HF200" },
    { int(0x4007d98a), "F S10/S100" },
    { int(0x4007da8e), "F R10/R16/R17/R18/R100/R106" },
    { int(0x4007da8f), "F M30/M31/M36/M300/M306" },
    { int(0x4007da90), "F S20/S21/S200" },
    { int(0x4007da92), "S31/FS36/FS37/FS300/FS305/FS306/FS307" },
    { int(0x4007dda9), "F G25" },
    { int(0x4007dfb4), "C10" },
    { int(0x80000001), "OS-1D" },
    { int(0x80000167), "OS-1DS" },
    { int(0x80000168), "OS 10D" },
    { int(0x80000169), "OS-1D Mark III" },
    { int(0x80000170), "OS Digital Rebel / 300D / Kiss Digital" },
    { int(0x80000174), "OS-1D Mark II" },
    { int(0x80000175), "OS 20D" },
    { int(0x80000176), "OS Digital Rebel XSi / 450D / Kiss X2" },
    { int(0x80000188), "OS-1Ds Mark II" },
    { int(0x80000189), "OS Digital Rebel XT / 350D / Kiss Digital N" },
    { int(0x80000190), "OS 40D" },
    { int(0x80000213), "OS 5D" },
    { int(0x80000215), "OS-1Ds Mark III" },
    { int(0x80000218), "OS 5D Mark II" },
    { int(0x80000219), "FT-E1" },
    { int(0x80000232), "OS-1D Mark II N" },
    { int(0x80000234), "OS 30D" },
    { int(0x80000236), "OS Digital Rebel XTi / 400D / Kiss Digital X" },
    { int(0x80000241), "FT-E2" },
    { int(0x80000246), "FT-E3" },
    { int(0x80000250), "OS 7D" },
    { int(0x80000252), "OS Rebel T1i / 500D / Kiss X3" },
    { int(0x80000254), "OS Rebel XS / 1000D / Kiss F" },
    { int(0x80000261), "OS 50D" },
    { int(0x80000269), "OS-1D X" },
    { int(0x80000270), "OS Rebel T2i / 550D / Kiss X4" },
    { int(0x80000271), "FT-E4" },
    { int(0x80000273), "FT-E5" },
    { int(0x80000281), "OS-1D Mark IV" },
    { int(0x80000285), "OS 5D Mark III" },
    { int(0x80000286), "OS Rebel T3i / 600D / Kiss X5" },
    { int(0x80000287), "OS 60D" },
    { int(0x80000288), "OS Rebel T3 / 1100D / Kiss X50" },
    { int(0x80000289), "OS 7D Mark II" },
    { int(0x80000297), "FT-E2 II" },
    { int(0x80000298), "FT-E4 II" },
    { int(0x80000301), "OS Rebel T4i / 650D / Kiss X6i" },
    { int(0x80000302), "OS 6D" },
    { int(0x80000324), "OS-1D C" },
    { int(0x80000325), "OS 70D" },
    { int(0x80000326), "OS Rebel T5i / 700D / Kiss X7i" },
    { int(0x80000327), "OS Rebel T5 / 1200D / Kiss X70" },
    { int(0x80000328), "OS-1D X MARK II" },
    { int(0x80000331), "OS M" },
    { int(0x80000346), "OS Rebel SL1 / 100D / Kiss X7" },
    { int(0x80000347), "OS Rebel T6s / 760D / 8000D" },
    { int(0x80000349), "OS 5D Mark IV" },
    { int(0x80000350), "OS 80D" },
    { int(0x80000355), "OS M2" },
    { int(0x80000382), "OS 5DS" },
    { int(0x80000393), "OS Rebel T6i / 750D / Kiss X8i" },
    { int(0x80000401), "OS 5DS R" },
    { int(0x80000404), "OS Rebel T6 / 1300D / Kiss X80" },
    { int(0x80000405), "OS Rebel T7i / 800D / Kiss X9i" },
    { int(0x80000406), "OS 6D Mark II" },
    { int(0x80000408), "OS 77D / 9000D" },
    { int(0x80000417), "OS Rebel SL2 / 200D / Kiss X9" },
    { -1, nullptr }
};

#if 0
static LabelIndex canon_xxx_table[] = {
    { 0, "" },
    { 1, "" },
    { 2, "" },
    { 3, "" },
    { 4, "" },
    { 5, "" },
    { 6, "" },
    { 7, "" },
    { 8, "" },
    { 9, "" },
    { 10, "" },
    { 11, "" },
    { 12, "" },
    { -1, nullptr }
};
#endif


static const ExplanationTableEntry canon_explanations[] = {
    { "Canon:MacroMode", explain_labeltable, canon_macromode_table },
    { "Canon:Quality", explain_labeltable, canon_quality_table },
    { "Canon:FlashMode", explain_labeltable, canon_flashmode_table },
    { "Canon:ContinuousDrive", explain_labeltable, canon_continuousdrive_table },
    { "Canon:FocusMode", explain_labeltable, canon_focusmode_table },
    { "Canon:RecordMode", explain_labeltable, canon_recordmode_table },
    { "Canon:ImageSize", explain_labeltable, canon_imagesize_table },
    { "Canon:EasyMode", explain_labeltable, canon_easymode_table },
    { "Canon:DigitalZoom", explain_labeltable, canon_digitalzoom_table },
    { "Canon:MeteringMode", explain_labeltable, canon_meteringmode_table },
    { "Canon:FocusRange", explain_labeltable, canon_focusrange_table },
    { "Canon:AFPoint", explain_labeltable, canon_afpoint_table },
    { "Canon:ExposureMode", explain_labeltable, canon_exposuremode_table },
    { "Canon:FlashBits", explain_canon_flashbits, nullptr },
    { "Canon:FocusContinuous", explain_labeltable, canon_focuscontinuous_table },
    { "Canon:AESetting", explain_labeltable, canon_aesetting_table },
    { "Canon:ImageStabilization", explain_labeltable, canon_imagestabilization_table },
    { "Canon:SpotMeteringMode", explain_labeltable, canon_spotmeteringmode_table },
    { "Canon:PhotoEffect", explain_labeltable, canon_photoeffect_table },
    { "Canon:ManualFlashOutput", explain_labeltable, canon_manualflashoutput_table },
    { "Canon:SRAWQuality", explain_labeltable, canon_srawquality_table },
    { "Canon:SlowShutter", explain_labeltable, canon_slowshutter_table },
    { "Canon:AFPointsInFocus", explain_labeltable, canon_afpointsinfocus_table },
    { "Canon:AutoExposureBracketing", explain_labeltable, canon_autoexposurebracketing_table },
    { "Canon:ControlMode", explain_labeltable, canon_controlmode_table },
    { "Canon:CameraType", explain_labeltable, canon_cameratype_table },
    { "Canon:AutoRotate", explain_labeltable, canon_autorotate_table },
    { "Canon:NDFilter", explain_labeltable, canon_ndfilter_table },
    { "Canon:WhiteBalance", explain_labeltable, canon_whitebalance_table },
    { "Canon:ModelID", explain_labeltable, canon_modelid_table },
    // { "Canon:", explain_labeltable, canon__table },
};



cspan<ExplanationTableEntry>
canon_explanation_table ()
{
    return cspan<ExplanationTableEntry>(canon_explanations);
}


/////////////////////////////////////////////////////////////////////////



// Put a whole bunch of sub-indexed data into the spec.
template<typename T>
inline void
array_to_spec (ImageSpec& spec,                 // spec to put attribs into
               const TIFFDirEntry& dir,         // TIFF dir entry
               cspan<uint8_t> buf,              // raw buffer blob
               cspan<LabelIndex> indices,       // LabelIndex table
               int offset_adjustment,
               bool swapendian,
               int na_value = std::numeric_limits<int>::max())
{
    // Make sure it's the right tag type. Be tolerant of signed/unsigned
    // mistakes.
    if (std::is_same<T,int16_t>::value || std::is_same<T,uint16_t>::value) {
        if ((dir.tdir_type != TIFF_SHORT && dir.tdir_type != TIFF_SSHORT))
            return;
    } else if (std::is_same<T,int32_t>::value || std::is_same<T,uint32_t>::value) {
        if ((dir.tdir_type != TIFF_LONG && dir.tdir_type != TIFF_SLONG))
            return;
    }
    else {
        OIIO_ASSERT(0 && "unsupported type");
        return;
    }
    const T *s = (const T *) pvt::dataptr (dir, buf, offset_adjustment);
    if (!s)
        return;
    for (auto&& attr : indices) {
        if (attr.value < int(dir.tdir_count)) {
            T ival = s[attr.value];
            if (swapendian)
                swap_endian(&ival, 1);
            if (ival != na_value)
                spec.attribute (attr.label, ival);
        }
    }
}


static LabelIndex canon_camerasettings_indices[] = {
    { 1, "Canon:MacroMode" },
    { 2, "Canon:SelfTimer" },
    { 3, "Canon:Quality" },
    { 4, "Canon:FlashMode" },
    { 5, "Canon:ContinuousDrive" },
    { 7, "Canon:FocusMode" },
    { 9, "Canon:RecordMode" },
    { 10, "Canon:ImageSize" },
    { 11, "Canon:EasyMode" },
    { 12, "Canon:DigitalZoom" },
    { 13, "Canon:Contrast" },
    { 14, "Canon:Saturation" },
    { 15, "Canon:Sharpness" },
    { 16, "Canon:CameraISO" },
    { 17, "Canon:MeteringMode" },
    { 18, "Canon:FocusRange" },
    { 19, "Canon:AFPoint" },
    { 20, "Canon:ExposureMode" },
    { 22, "Canon:LensType" },
    { 23, "Canon:MaxFocalLength" },
    { 24, "Canon:MinFocalLength" },
    { 25, "Canon:FocalUnits" },
    { 26, "Canon:MaxAperture" },
    { 27, "Canon:MinAperture" },
    { 28, "Canon:FlashActivity" },
    { 29, "Canon:FlashBits" },
    { 32, "Canon:FocusContinuous" },
    { 33, "Canon:AESetting" },
    { 34, "Canon:ImageStabilization" },
    { 35, "Canon:DisplayAperture" },
    { 36, "Canon:ZoomSourceWidth" },
    { 37, "Canon:ZoomTargetWidth" },
    { 39, "Canon:SpotMeteringMode" },
    { 40, "Canon:PhotoEffect" },
    { 41, "Canon:ManualFlashOutput" },
    { 42, "Canon:ColorTone" },
    { 46, "Canon:SRAWQuality" }
};

static void
canon_camerasettings_handler (const TagInfo& /*taginfo*/, const TIFFDirEntry& dir,
                              cspan<uint8_t> buf, ImageSpec& spec,
                              bool swapendian, int offset_adjustment)
{
    array_to_spec<int16_t> (spec, dir, buf, canon_camerasettings_indices,
                            offset_adjustment, swapendian, -1);
}


static LabelIndex canon_focallength_indices[] = {
    { 0, "Canon:FocalType" },
    { 1, "Canon:FocalLength" },
    { 2, "Canon:FocalPlaneXSize" },
    { 3, "Canon:FocalPlaneYSize" }
};


static void
canon_focallength_handler (const TagInfo& /*taginfo*/, const TIFFDirEntry& dir,
                           cspan<uint8_t> buf, ImageSpec& spec,
                           bool swapendian, int offset_adjustment)
{
    array_to_spec<uint16_t> (spec, dir, buf, canon_focallength_indices,
                             offset_adjustment, swapendian);
}


static LabelIndex canon_shotinfo_indices[] = {
    { 1, "Canon:AutoISO" },
    { 2, "Canon:BaseISO" },
    { 3, "Canon:MeasuredEV" },
    { 4, "Canon:TargetAperture" },
    { 5, "Canon:TargetExposureTime" },
    { 6, "Canon:ExposureCompensation" },
    { 7, "Canon:WhiteBalance" },
    { 8, "Canon:SlowShutter" },
    { 9, "Canon:SequenceNumber" },
    { 10, "Canon:OpticalZoomCode" },
    { 12, "Canon:CameraTemperature" },
    { 13, "Canon:FlashGuideNumber" },
    { 14, "Canon:AFPointsInFocus" },
    { 15, "Canon:ExposureComp" },
    { 16, "Canon:FlashExposureComp" },
    { 17, "Canon:AutoExposureBracketing" },
    { 18, "Canon:AEBBracketValue" },
    { 19, "Canon:ControlMode" },
    { 20, "Canon:FocusDistanceUpper" },
    { 21, "Canon:FocusDistanceLower" },
    { 22, "Canon:FNumber" },
    { 23, "Canon:ExposureTime" },
    { 24, "Canon:MeasuredEV2" },
    { 25, "Canon:BulbDuration" },
    { 26, "Canon:CameraType" },
    { 27, "Canon:AutoRotate" },
    { 28, "Canon:NDFilter" },
    { 29, "Canon:SelfTimer2" },
    { 33, "Canon:FlashOutput" },
};

static void
canon_shotinfo_handler (const TagInfo& /*taginfo*/, const TIFFDirEntry& dir,
                        cspan<uint8_t> buf, ImageSpec& spec,
                        bool swapendian, int offset_adjustment)
{
    array_to_spec<int16_t> (spec, dir, buf, canon_shotinfo_indices,
                            offset_adjustment, swapendian);
}


static LabelIndex canon_panorama_indices[] = {
    { 2, "Canon:PanoramaFrameNumber" },
    { 5, "Canon:PanoramaDirection" },
};

static void
canon_panorama_handler (const TagInfo& /*taginfo*/, const TIFFDirEntry& dir,
                        cspan<uint8_t> buf, ImageSpec& spec,
                        bool swapendian, int offset_adjustment)
{
    array_to_spec<int16_t> (spec, dir, buf, canon_panorama_indices,
                            offset_adjustment, swapendian);
}

static LabelIndex canon_sensorinfo_indices[] = {
    { 1, "Canon:SensorWidth" },
    { 2, "Canon:SensorHeight" },
    { 5, "Canon:SensorLeftBorder" },
    { 6, "Canon:SensorTopBorder" },
    { 7, "Canon:SensorRightBorder" },
    { 8, "Canon:SensorBottomBorder" },
    { 9, "Canon:BlackMaskLeftBorder" },
    { 10, "Canon:BlackMaskTopBorder" },
    { 11, "Canon:BlackMaskRightBorder" },
    { 12, "Canon:BlackMaskBottomBorder" },
};

static void
canon_sensorinfo_handler (const TagInfo& /*taginfo*/, const TIFFDirEntry& dir,
                          cspan<uint8_t> buf, ImageSpec& spec,
                          bool swapendian, int offset_adjustment)
{
    array_to_spec<uint16_t> (spec, dir, buf, canon_sensorinfo_indices,
                             offset_adjustment, swapendian);
}



enum CANON_TAGS {
    CANON_CAMERASETTINGS    = 0X0001,
    CANON_FOCALLENGTH       = 0X0002,
    CANON_FLASHINFO         = 0X0003,
    CANON_SHOTINFO          = 0X0004,
    CANON_PANORAMA          = 0X0005,
    CANON_IMAGETYPE         = 0X0006,
    CANON_FIRMWAREVERSION   = 0X0007,
    CANON_FILENUMBER        = 0X0008,
    CANON_OWNERNAME         = 0X0009,
    CANON_SERIALNUMBER      = 0X000C,
    CANON_CAMERAINFO        = 0X000D,
    CANON_FILELENGTH        = 0X000E,
    CANON_CUSTOMFUNCTIONS   = 0X000F,
    CANON_MODELID           = 0X0010,
    CANON_MOVIEINFO         = 0X0011,
    CANON_AFINFO            = 0X0012,
    CANON_THUMBNAILIMAGEVALIDAREA = 0X0013,
    CANON_SERIALNUMBERFORMAT = 0X0015,
    CANON_SUPERMACRO        = 0X001A,
    CANON_DATESTAMPMODE     = 0X001C,
    CANON_MYCOLORS          = 0X001D,
    CANON_FIRMWAREREVISION  = 0X001E,
    CANON_CATEGORIES        = 0X0023,
    CANON_FACEDETECT1       = 0X0024,
    CANON_FACEDETECT2       = 0X0025,
    CANON_AFINFO2           = 0X0026,
    CANON_CONTRASTINFO      = 0X0027,
    CANON_IMAGEUNIQUEID     = 0X0028,
    CANON_FACEDETECT3       = 0X002F,
    CANON_TIMEINFO          = 0X0035,
    CANON_AFINFO3           = 0X003C,
    CANON_ORIGINALDECISIONDATAOFFSET = 0X0083,
    CANON_CUSTOMFUNCTIONS1D = 0X0090,
    CANON_PERSONALFUNCTIONS = 0X0091,
    CANON_PERSONALFUNCTIONVALUES    = 0X0092,
    CANON_FILEINFO          = 0X0093,
    CANON_AFPOINTSINFOCUS1D = 0X0094,
    CANON_LENSMODEL         = 0X0095,
    CANON_SERIALINFO        = 0X0096,
    CANON_DUSTREMOVALDATA   = 0X0097,
    CANON_CROPINFO          = 0X0098,
    CANON_CUSTOMFUNCTIONS2  = 0X0099,
    CANON_ASPECTINFO        = 0X009A,
    CANON_PROCESSINGINFO    = 0X00a0,
    CANON_TONECURVETABLE    = 0X00A1,
    CANON_SHARPNESSTABLE    = 0X00A2,
    CANON_SHARPNESSFREQTABLE = 0X00A3,
    CANON_WHITEBALANCETABLE = 0X00A4,
    CANON_COLORBALANCE      = 0X00A9,
    CANON_MEASUREDCOLOR     = 0X00AA,
    CANON_COLORTEMPERATURE  = 0X00AE,
    CANON_CANONFLAGS        = 0X00B0,
    CANON_MODIFIEDINFO      = 0X00B1,
    CANON_TONECURVEMATCHING = 0X00B2,
    CANON_WHITEBALANCEMATCHING  = 0X00B3,
    CANON_COLORSPACE        = 0X00B4,
    CANON_PREVIEWIMAGEINFO  = 0X00B6,
    CANON_VRDOFFSET         = 0X00D0,
    CANON_SENSORINFO        = 0X00E0,
    CANON_COLORDATA         = 0X4001,
    CANON_CRWPARAM          = 0X4002,
    CANON_FLAVOR            = 0X4005,
    CANON_PICTURESTYLEUSERDEF = 0X4008,
    CANON_PICTURESTYLEPC    = 0X4009,
    CANON_CUSTOMPICTURESTYLEFILENAME = 0X4010,
    CANON_AFMICROADJ        = 0X4013,
    CANON_VIGNETTINGCORR    = 0X4015,
    CANON_VIGNETTINGCORR2   = 0X4016,
    CANON_LIGHTINGOPT       = 0X4018,
    CANON_LENSINFO          = 0X4019,
    CANON_AMBIENCEINFO      = 0X4020,
    CANON_MULTIEXP          = 0X4021,
    CANON_FILTERINFO        = 0X4024,
    CANON_HDRINFO           = 0X4025,
    CANON_AFCONFIG          = 0X4028
};



static const TagInfo canon_maker_tag_table[] = {
    { CANON_CAMERASETTINGS, "Canon:CameraSettings",  TIFF_SHORT, 0, canon_camerasettings_handler },
    { CANON_FOCALLENGTH, "Canon:FocalLength",  TIFF_SHORT, 0, canon_focallength_handler },
    // { CANON_FLASHINFO, "Canon:FlashInfo", unknown }
    { CANON_SHOTINFO, "Canon:ShotInfo",  TIFF_SHORT, 0, canon_shotinfo_handler },
    { CANON_PANORAMA, "Canon:Panorama",  TIFF_SHORT, 0, canon_panorama_handler },
    { CANON_IMAGETYPE, "Canon:ImageType",       TIFF_ASCII, 0 },
    { CANON_FIRMWAREVERSION, "Canon:FirmwareVersion", TIFF_ASCII, 1 },
    { CANON_FILENUMBER, "Canon:FileNumber",      TIFF_LONG,  1 },
    { CANON_OWNERNAME, "Canon:OwnerName",       TIFF_ASCII, 0 },
    // { 0x000a, unknown }
    { CANON_SERIALNUMBER, "Canon:SerialNumber",    TIFF_LONG,  1 },
    // { CANON_CAMERAINFO, "Canon:CameraInfo",    TIFF_LONG,  0, canon_blah_handler },
    // { CANON_FILELENGTH, "Canon:FileLength",    TIFF_LONG,  1 },
    // { CANON_CUSTOMFUNCTIONS, "Canon:CustomFunctions",    TIFF_LONG,  0, canon_blah_handler },
    { CANON_MODELID, "Canon:ModelID",    TIFF_LONG,  1 },
    // { CANON_MOVIEINFO, "Canon:MovieInfo",    TIFF_LONG,  0, canon_blah_handler },
    // { CANON_AFINFO, "Canon:AFInfo",    TIFF_LONG,  0, canon_blah_handler },
    { CANON_THUMBNAILIMAGEVALIDAREA, "Canon:ThumbnailImageValidArea", TIFF_LONG, 4 },
    { CANON_SERIALNUMBERFORMAT, "Canon:SerialNumberFormat", TIFF_LONG, 1 },
    { CANON_SUPERMACRO, "Canon:SuperMacro", TIFF_SHORT, 1 },
    { CANON_DATESTAMPMODE, "Canon:DateStampMode", TIFF_SHORT, 1 },
    // { CANON_MYCOLORS, "Canon:MyColors",    TIFF_NOTYPE,  0, canon_blah_handler },
    { CANON_FIRMWAREREVISION, "Canon:FirmwareRevision", TIFF_LONG, 1 },
    { CANON_CATEGORIES, "Canon:Categories", TIFF_LONG, 2 },
    // { CANON_FACEDETECT1, "Canon:FaceDetect1",    TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_FACEDETECT2, "Canon:FaceDetect2",    TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_AFINFO2, "Canon:AFInfo2",    TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_CONTRASTINFO, "Canon:ContrastInfo",    TIFF_NOTYPE,  0, canon_blah_handler },
    { CANON_IMAGEUNIQUEID, "Canon:ImageUniqueID",    TIFF_BYTE,  1 },
    // { CANON_FACEDETECT3, "Canon:FaceDetect3",    TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_TIMEINFO, "Canon:TimeInfo",    TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_AFINFO3, "Canon:AFInfo3",    TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_ORIGINALDECISIONDATAOFFSET, "Canon:OriginalDecisionDataOffset", TIFF_NOTYPE,  0 },
    // { CANON_CUSTOMFUNCTIONS1D, "Canon:CustomFunctions1D", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_PERSONALFUNCTIONS, "Canon:PersonalFunctions", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_PERSONALFUNCTIONVALUES, "Canon:PersonalFunctionValues", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_FILEINFO, "Canon:FileInfo", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_AFPOINTSINFOCUS1D, "Canon:AFPointsInFocus1D", TIFF_NOTYPE,  0, canon_blah_handler },
    { CANON_LENSMODEL, "Canon:LensModel", TIFF_ASCII, 1 },
    // { CANON_SERIALINFO, "Canon:SerialInfo", TIFF_NOTYPE, 0, canon_blah_handler },
    // { CANON_DUSTREMOVALDATA, "Canon:DustRemovalData", TIFF_NOTYPE, 0, canon_blah_handler }, // unknown format
    { CANON_CROPINFO, "Canon:CropInfo", TIFF_SHORT, 4 },
    // { CANON_CUSTOMFUNCTIONS2, "Canon:CustomFunctions2", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_ASPECTINFO, "Canon:AspectInfo", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_PROCESSINGINFO, "Canon:ProcessingInfo", TIFF_NOTYPE,  0, canon_blah_handler },
    // CANON_TONECURVETABLE, ToneCurveTable
    // CANON_SHARPNESSTABLE, SharpnessTable
    // CANON_SHARPNESSFREQTABLE, SharpnessFreqTable
    // CANON_WHITEBALANCETABLE, WhiteBalanceTable
    // { CANON_COLORBALANCE, "Canon:ColorBalance", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_MEASUREDCOLOR, "Canon:MeasuredColor", TIFF_NOTYPE,  0, canon_blah_handler },
    { CANON_COLORTEMPERATURE, "Canon:ColorTemperature", TIFF_SHORT, 1 },
    // { CANON_CANONFLAGS, "Canon:CanonFlags", TIFF_NOTYPE,  0, canon_blah_handler },
    // { CANON_MODIFIEDINFO, "Canon:ModifiedInfo", TIFF_NOTYPE,  0, canon_blah_handler },
    // CANON_TONECURVEMATCHING, ToneCurveMatching
    // CANON_WHITEBALANCEMATCHING, WhiteBalanceMatching
    // CANON_COLORSPACE, ColorSpace, TIFF_SHORT, // 1=sRGB, 2=Adobe RGB
    // CANON_PREVIEWIMAGEINFO, PreviewImageInfo
    // CANON_VRDOFFSET, VRDOffset
    { CANON_SENSORINFO, "Canon:SensorInfo", TIFF_SHORT, 17, canon_sensorinfo_handler },
    // CANON_COLORDATA ColorData    varies by model
    // CANON_CRWPARAM CRWParam
    // CANON_FLAVOR Flavor
    // CANON_PICTURESTYLEUSERDEF, PictureStyleUserDef
    // CANON_PICTURESTYLEPC, PictureStylePC
    { CANON_CUSTOMPICTURESTYLEFILENAME, "Canon:CustomPictureStyleFileName", TIFF_ASCII, 1 },
    // CANON_AFMICROADJ, AFMicroAdj
    // CANON_VIGNETTINGCORR, VignettingCorr     varies by model
    // CANON_VIGNETTINGCORR2, VignettingCorr2
    // CANON_LIGHTINGOPT, LightingOpt
    // CANON_LENSINFO, LensInfo
    // CANON_AMBIENCEINFO, AmbienceInfo
    // CANON_MULTIEXP, MultiExp
    // CANON_FILTERINFO, FilterInfo
    // CANON_HDRINFO, HDRInfo
    // CANON_AFCONFIG, AFConfig
};



const TagMap& canon_maker_tagmap_ref () {
    static TagMap T ("Canon", canon_maker_tag_table);
    return T;
}



// Put a whole bunch of sub-indexed data into the spec into the given TIFF
// tag.
template<typename T>
static void
encode_indexed_tag (int tifftag, TIFFDataType tifftype, // TIFF tag and type
                    cspan<LabelIndex> indices,        // LabelIndex table
                    std::vector<char>& data,          // data blob to add to
                    std::vector<TIFFDirEntry> &dirs,  // TIFF dirs to add to
                    const ImageSpec& spec,            // spec to get attribs from
                    size_t offset_correction)         // offset correction
{
    // array length is determined by highest index value
    std::vector<T> array (indices.back().value + 1, T(0));
    bool anyfound = false;
    for (auto&& attr : indices) {
        if (attr.value < int(array.size())) {
            const ParamValue *param = spec.find_attribute (attr.label);
            if (param) {
                array[attr.value] = T (param->get_int());
                anyfound = true;
            }
        }
    }
    if (anyfound)
        append_tiff_dir_entry (dirs, data, tifftag, tifftype,
                               array.size(), array.data(), offset_correction);
}




void
encode_canon_makernote (std::vector<char>& data,
                        std::vector<TIFFDirEntry> &makerdirs,
                        const ImageSpec& spec, size_t offset_correction)
{
    // Easy ones that get coded straight from the attribs
    for (const TagInfo& t : canon_maker_tag_table) {
        if (t.handler)   // skip  ones with handlers
            continue;
        if (const ParamValue* param = spec.find_attribute (t.name)) {
            size_t count = t.tiffcount;
            const void* d = param->data();
            if (t.tifftype == TIFF_ASCII) {
                // special case: strings need their real length, plus
                // trailing null, and the data must be the characters.
                d = param->get_ustring().c_str();
                count = param->get_ustring().size() + 1;
            }
            append_tiff_dir_entry (makerdirs, data, t.tifftag, t.tifftype,
                                   count, d, offset_correction);
        }
    }

    // Hard ones that need to fill in complicated structures
    encode_indexed_tag<int16_t> (CANON_CAMERASETTINGS, TIFF_SSHORT,
                                 canon_camerasettings_indices,
                                 data, makerdirs, spec, offset_correction);
    encode_indexed_tag<uint16_t> (CANON_FOCALLENGTH, TIFF_SHORT,
                                 canon_focallength_indices,
                                 data, makerdirs, spec, offset_correction);
    encode_indexed_tag<int16_t> (CANON_SHOTINFO, TIFF_SSHORT,
                                 canon_shotinfo_indices,
                                 data, makerdirs, spec, offset_correction);
    encode_indexed_tag<int16_t> (CANON_SHOTINFO, TIFF_SSHORT,
                                 canon_shotinfo_indices,
                                 data, makerdirs, spec, offset_correction);
    encode_indexed_tag<int16_t> (CANON_PANORAMA, TIFF_SSHORT,
                                 canon_panorama_indices,
                                 data, makerdirs, spec, offset_correction);
}


}  // end namespace pvt
OIIO_NAMESPACE_END
