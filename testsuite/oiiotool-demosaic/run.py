#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = " >> out.txt 2>&1"

width = 320
height = 240

make_test_image = "--pattern fill:topleft=0,0,1:topright=0,1,0:bottomleft=1,0,1:bottomright=1,1,0 {}x{} 3 --dup ".format(width, height)

make_pattern_RGGB = "--pattern constant:color=0,1,0 {}x{} 3 -for y \"0,{{TOP.height}},2\" -for x \"0,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x+1}},{{y+1}}\" -endfor -endfor ".format(width, height)

make_pattern_GRBG = "--pattern constant:color=0,1,0 {}x{} 3 -for y \"0,{{TOP.height}},2\" -for x \"1,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x-1}},{{y+1}}\" -endfor -endfor ".format(width, height)

make_pattern_GBRG = "--pattern constant:color=0,1,0 {}x{} 3 -for y \"1,{{TOP.height}},2\" -for x \"0,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x+1}},{{y-1}}\" -endfor -endfor ".format(width, height)

make_pattern_BGGR = "--pattern constant:color=0,1,0 {}x{} 3 -for y \"1,{{TOP.height}},2\" -for x \"1,{{TOP.width}},2\" -point:color=1,0,0 \"{{x}},{{y}}\" -point:color=0,0,1 \"{{x-1}},{{y-1}}\" -endfor -endfor ".format(width, height)



mosaic = ""
test = " --fail 0.006 --hardfail 0.01 --failpercent 2 --warn 0.001 --diff "

print(make_test_image + make_pattern_RGGB + "--mul --chsum ")

layouts = {
    "RGGB": make_pattern_RGGB,
    "GRBG": make_pattern_GRBG,
    "GBRG": make_pattern_GBRG,
    "BGGR": make_pattern_BGGR
}

for algo in ['linear', 'MHC']:
    for k, v in layouts.items():
        command += oiiotool (make_test_image + v + "--mul --chsum " + "--demosaic:algorithm={}:layout={} ".format(algo, k) + test)
