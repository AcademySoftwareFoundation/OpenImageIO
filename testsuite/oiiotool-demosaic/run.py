#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = " >> out.txt 2>&1"

width = 160
height = 120

# oiiotool commands to make each of the Bayer patterns
make_pattern_RGGB = f"--pattern constant:color=0,1,0 {width}x{height} 3 -for y \"0,{{TOP.height}},2\" -for x \"0,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x+1}},{{y+1}}\" -endfor -endfor"
make_pattern_GRBG = f"--pattern constant:color=0,1,0 {width}x{height} 3 -for y \"0,{{TOP.height}},2\" -for x \"1,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x-1}},{{y+1}}\" -endfor -endfor"
make_pattern_GBRG = f"--pattern constant:color=0,1,0 {width}x{height} 3 -for y \"1,{{TOP.height}},2\" -for x \"0,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x+1}},{{y-1}}\" -endfor -endfor"
make_pattern_BGGR = f"--pattern constant:color=0,1,0 {width}x{height} 3 -for y \"1,{{TOP.height}},2\" -for x \"1,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x-1}},{{y-1}}\" -endfor -endfor"

layouts = {
    "RGGB": make_pattern_RGGB,
    "GRBG": make_pattern_GRBG,
    "GBRG": make_pattern_GBRG,
    "BGGR": make_pattern_BGGR
}

# Create a test image with color gradients
make_testimage = f"--pattern fill:topleft=0,0,1:topright=0,1,0:bottomleft=1,0,1:bottomright=1,1,0 {width}x{height} 3 "


types = [
    {
        "type" : "float",
        "ext" : "exr",
        "threshold": 0.0000003
    },
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

    

for dict in types:

    type = dict["type"]
    ext = dict["ext"]
    threshold = dict["threshold"]

    command += oiiotool (make_testimage + f" -o:type={type} testimage_{type}.{ext}")

    # For each Bayer pattern (RGGB, RGRB, GBRG, BGGR), create an image with that
    # pure pattern ({pattern}.exr), then multiply it by the test image and take
    # the channel sum to get a Bayer mosaic image ({pattern}-bayer.exr).
    for pattern, maker in layouts.items():
        command += oiiotool (f"{maker} -o:type={type} pattern_{type}_{pattern}.{ext} testimage_{type}.{ext} -mul -chsum -o:type={type} bayer_{type}_{pattern}.{ext}")

    test = f" --fail {threshold} --hardfail {threshold} --warn {threshold} --diff "


    # For each algorithm, try demosaicing each pattern test image and compare to
    # the original test image.
    for algo in ['linear', 'MHC']:
        for pattern, maker in layouts.items():
            command += oiiotool (f"-i:type={type} testimage_{type}.{ext} -i:type={type} bayer_{type}_{pattern}.{ext} --demosaic:algorithm={algo}:layout={pattern} -o:type={type} result_{type}_{pattern}-{algo}.{ext} ")
            
            crop = 2
            
            if crop > 0:
                cut_cmd = f"-cut {{TOP.width-{crop}*2}}x{{TOP.height-{crop}*2}}+{{TOP.x+{crop}}}+{{TOP.y+{crop}}} "
            else:
                cut_cmd = ""
                        
            command += oiiotool (f"testimage_{type}.{ext} " + cut_cmd + f"result_{type}_{pattern}-{algo}.{ext} " + cut_cmd + test)
