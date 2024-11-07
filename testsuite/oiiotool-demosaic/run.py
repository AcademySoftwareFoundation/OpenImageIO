#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = " >> out.txt 2>&1"

width = 160
height = 120

# Create a test image with color gradient
make_testimage = f"--pattern fill:topleft=0,0,1:topright=0,1,0:bottomleft=1,0,1:bottomright=1,1,0 {width}x{height} 3 "
command += oiiotool (make_testimage + f" -o:type=float testimage.exr")

tests = [
    # Making an un-white-balanced test image is difficult, especially with
    # split weights on the green sub-channels. Un-white-balancing the intermediate
    # bayer images makes that step contribute to the error. The smaller data
    # types don't round-trip cleanly, and that does not make a fair test anyway.
    # So we only test white-balancing on the float type.
    {
        'suffix' : '_WB',
        'WB' : [1.8, 0.8, 1.2, 1.5],
        'types' : [
            {
                "type" : "float",
                "ext" : "exr",
                "threshold": 0.0000003
            }
        ]
    },
    {
        'suffix' : '',
        'WB' : [1.0, 1.0, 1.0, 1.0],
        'types' : [
            {
                "type" : "half",
                "ext" : "exr",
                "threshold": 0.0005
            },
            {
                "type" : "uint16",
                "ext" : "tiff",
                "threshold": 0.000016
            },
            {
                "type" : "uint8",
                "ext" : "tiff",
                "threshold": 0.004
            }
        ]
    }
]

def make_pattern(x_offset, y_offset):
    ri  = ((0 + x_offset) % 2) + 2 * ((0 + y_offset) % 2)
    g1i = ((1 + x_offset) % 2) + 2 * ((0 + y_offset) % 2)
    g2i = ((0 + x_offset) % 2) + 2 * ((1 + y_offset) % 2)
    bi  = ((1 + x_offset) % 2) + 2 * ((1 + y_offset) % 2)
    
    pattern = '    '
    pattern = pattern[:ri ] + 'R' + pattern[ri  + 1:]
    pattern = pattern[:g1i] + 'G' + pattern[g1i + 1:]
    pattern = pattern[:g2i] + 'G' + pattern[g2i + 1:]
    pattern = pattern[:bi ] + 'B' + pattern[bi  + 1:]
    return pattern
    

for dict in tests:
    suffix = dict['suffix']

    WB_R  = dict['WB'][0]
    WB_G1 = dict['WB'][1]
    WB_G2 = dict['WB'][2]
    WB_B  = dict['WB'][3]

    # For each Bayer pattern (RGGB, RGRB, GBRG, BGGR), create an image with that
    # pure pattern ({pattern}.exr), then multiply it by the test image and take
    # the channel sum to get a Bayer mosaic image ({pattern}-bayer.exr).
    # This is somewhat expensive, so we do it only once (for float) and then will
    # convert it to the appropriate type upon input.
    for yy in range(0, 2):
        for xx in range(0, 2):

            pattern = make_pattern(xx, yy)

            point_r  = f"-point:color={1.0 / WB_R},0,0  \"{{x+({xx}+0) % 2}},{{y+({yy}+0) % 2}}\""
            point_g1 = f"-point:color=0,{1.0 / WB_G1},0 \"{{x+({xx}+1) % 2}},{{y+({yy}+0) % 2}}\""
            point_g2 = f"-point:color=0,{1.0 / WB_G2},0 \"{{x+({xx}+0) % 2}},{{y+({yy}+1) % 2}}\""
            point_b  = f"-point:color=0,0,{1.0 / WB_B}  \"{{x+({xx}+1) % 2}},{{y+({yy}+1) % 2}}\""

            maker = f"--pattern constant:color=0,0,0 {width}x{height} 3 -for y \"0,{{TOP.height}},2\" -for x \"0,{{TOP.width}},2\" {point_r} {point_g1} {point_g2} {point_b} -endfor -endfor"
            command += oiiotool (f"{maker} -o:type=float pattern_{pattern}{suffix}.exr testimage.exr -mul -chsum -o:type=float bayer_{pattern}{suffix}.exr")

            for type_dict in dict["types"]:

                type = type_dict["type"]
                ext = type_dict["ext"]
                threshold = type_dict["threshold"]

                test = f" --fail {threshold} --hardfail {threshold} --warn {threshold} --diff "

                # For each algorithm, try demosaicing each pattern test image and compare to
                # the original test image.
                for algo in ['linear', 'MHC']:

                    command += oiiotool (f"-i:type={type} testimage.exr -i:type={type} bayer_{pattern}{suffix}.exr --demosaic:algorithm={algo}:layout={pattern}:white_balance={WB_R},{WB_G1},{WB_B},{WB_G2} -o:type={type} result_{type}_{pattern}{suffix}-{algo}.{ext} ")

                    crop = 2

                    if crop > 0:
                        cut_cmd = f"-cut {{TOP.width-{crop}*2}}x{{TOP.height-{crop}*2}}+{{TOP.x+{crop}}}+{{TOP.y+{crop}}} "
                    else:
                        cut_cmd = ""

                    command += oiiotool (f"testimage.exr " + cut_cmd + f"result_{type}_{pattern}{suffix}-{algo}.{ext} " + cut_cmd + test)
