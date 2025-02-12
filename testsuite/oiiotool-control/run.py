#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Create some test images we need
# command += oiiotool ("--create 320x240 3 -d uint8 -o black.tif")
# command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 128x128 3 -d half -o grey128.exr")
# command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 64x64 3 -d half -o grey64.exr")
# command += oiiotool ("--create 256x256 3 --fill:color=1,.5,.5 256x256 --fill:color=0,1,0 80x80+100+100 -d uint8 -o filled.tif")
# wrapper_cmd = "time"
redirect += " 2>&1"
failureok = True

# Make some temp files
command += oiiotool ('-pattern:type=uint8 constant:color=1,0,0 2x2 3 -o a.tif ' +
                     '-pattern:type=uint8 constant:color=0,1,0 2x2 3 -o b.tif ' +
                     '-pattern:type=uint8 constant:color=0,0,1 2x2 3 -o c.tif ' +
                     '-pattern:type=uint8 constant:color=1,1,1 2x2 3 -o d.tif ')

# Test TOP, BOTTOM, IMG[]
# TOP should be c.tif, BOTTOM should be a.tif
command += oiiotool ("a.tif b.tif c.tif d.tif " +
                     "--echo \"Stack holds [0] = {IMG[0].filename}, [1] = {IMG[1].filename}, [2] = {IMG[2].filename}\" " +
                     "--echo \"TOP = {TOP.filename}, BOTTOM = {BOTTOM.filename}\" "
                     )
# Test --pop, --popbottom, --stackreverse, --stackclear, --stackextract
command += oiiotool (
      "a.tif b.tif c.tif d.tif "
    + "--echo \"Stack bottom to top:\" "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"after --stackreverse:\" --stackreverse "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"after --stackreverse:\" --stackreverse "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"after --pop:\" --pop "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"after --popbottom:\" --popbottom "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"after --stackclear:\" --stackclear "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"Re-add a, b, c, d:\" "
    + "a.tif b.tif c.tif d.tif "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    + "--echo \"--stackextract 2:\" --stackextract 2 "
    + "--for i 0,{NIMAGES} --echo \"  {IMG[NIMAGES-1-i].filename}\" --endfor "
    )

# test expression substitution
command += oiiotool ('-echo "42+2 = {42+2}" ' +
                     '-echo "42-2 = {42-2}" ' +
                     '-echo "42*2 = {42*2}" ' +
                     '-echo "42/2 = {42/2}" ' +
                     '-echo "42<41 = {42<41}" ' +
                     '-echo "42<42 = {42<42}" ' +
                     '-echo "42<43 = {42<43}" ' +
                     '-echo "42<=41 = {42<=41}" ' +
                     '-echo "42<=42 = {42<=42}" ' +
                     '-echo "42<=43 = {42<=43}" ' +
                     '-echo "42>41 = {42>41}" ' +
                     '-echo "42>42 = {42>42}" ' +
                     '-echo "42>43 = {42>43}" ' +
                     '-echo "42>=41 = {42>=41}" ' +
                     '-echo "42>=42 = {42>=42}" ' +
                     '-echo "42>=43 = {42>=43}" ' +
                     '-echo "42==41 = {42==41}" ' +
                     '-echo "42==42 = {42==42}" ' +
                     '-echo "42==43 = {42==43}" ' +
                     '-echo "42!=41 = {42!=41}" ' +
                     '-echo "42!=42 = {42!=42}" ' +
                     '-echo "42!=43 = {42!=43}" ' +
                     '-echo "42<=>41 = {42<=>41}" ' +
                     '-echo "42<=>42 = {42<=>42}" ' +
                     '-echo "42<=>43 = {42<=>43}" ' +
                     '-echo "(1==2)&&(2==2) = {(1==2)&&(2==2)}" ' +
                     '-echo "(1==1)&&(2==2) = {(1==1)&&(2==2)}" ' +
                     '-echo "(1==2)&&(1==2) = {(1==2)&&(1==2)}" ' +
                     '-echo "(1==2)||(2==2) = {(1==2)||(2==2)}" ' +
                     '-echo "(1==1)||(2==2) = {(1==1)||(2==2)}" ' +
                     '-echo "(1==2)||(1==2) = {(1==2)||(1==2)}" ' +
                     '-echo "not(1==1) = {not(1==1)}" ' +
                     '-echo "not(1==2) = {not(1==2)}" ' +
                     '-echo "!(1==1) = {!(1==1)}" ' +
                     '-echo "!(1==2) = {!(1==2)}" ' +
                     '-echo "eq(foo,foo) = {eq(\'foo\',\'foo\')}" ' +
                     '-echo "eq(foo,bar) = {eq(\'foo\',\'bar\')}" ' +
                     '-echo "neq(foo,foo) = {neq(\'foo\',\'foo\')}" ' +
                     '-echo "neq(foo,bar) = {neq(\'foo\',\'bar\')}" ')

command += oiiotool ('-echo "16+5={16+5}" -echo "16-5={16-5}" -echo "16*5={16*5}"')
command += oiiotool ('-echo "16/5={16/5}" -echo "16//5={16//5}" -echo "16%5={16%5}"')
command += oiiotool ("../common/tahoe-small.tif --pattern fill:top=0,0,0,0:bottom=0,0,1,1 " +
                     "{TOP.geom} {TOP.nchannels} -d uint8 -o exprgradient.tif")
command += oiiotool ('../common/tahoe-small.tif -cut "{TOP.width-20* 2}x{TOP.height-40+(4*2- 2 ) /6-1}+{TOP.x+100.5-80.5 }+{TOP.y+20}" -d uint8 -o exprcropped.tif')
command += oiiotool ('../common/tahoe-small.tif -o exprstrcat{TOP.compression}.tif')
command += oiiotool ('../common/tahoe-tiny.tif -subc "{TOP.MINCOLOR}" -divc "{TOP.MAXCOLOR}" -o tahoe-contraststretch.tif')
# test use of quotes inside evaluation, {TOP.foo/bar} would ordinarily want
# to interpret '/' for division, but we want to look up metadata called
# 'foo/bar'.
command += oiiotool ("-create 16x16 3 -attrib \"foo/bar\" \"xyz\" -echo \"{TOP.'foo/bar'} should say xyz\"")
command += oiiotool ("-create 16x16 3 -attrib smpte:TimeCode \"01:02:03:04\" -echo \"timecode is {TOP.'smpte:TimeCode'}\"")

# Ensure that --evaloff/--evalon work
command += oiiotool ("-echo \"{1+1}\" --evaloff -echo \"{3+4}\" --evalon -echo \"{2*2}\"")

# Test user variables
command += oiiotool ('-echo "Testing --set, expr i:" -set i 1 -echo "  i = {i}" -set i "{i+41}" -echo "  now i = {i}"')
command += oiiotool ('-echo "Testing --set, expr var(i):" -set i 1 -echo "  i = {var(i)}" -set i "{i+41}" -echo "  now i = {var(i)}"')
command += oiiotool ('-echo "Testing --set of implied types:" ' +
                     '-set i 42 -set f 3.5 ' +
                     '-set s "hello world" ' +
                     '-echo "  i = {i}, f = {f}, s = {s}"')
command += oiiotool ('-echo "Testing --set of various explicit types:" ' +
                     '-set:type=int i 42 -set:type=float f 3.5 ' +
                     '-set:type=string s "hello world" ' +
                     '-set:type="string[3]" sarr "hello","world","wide" ' +
                     '-set:type=timecode tc 01:02:03:04 ' +
                     '-set:type=rational rat 1/2 ' +
                     '-echo "  i = {i}, f = {f}, s = {s}, tc = {tc}, rat = {rat}"')
command += oiiotool ('-echo "This should make an error:" ' +
                     '-set 3 5')

# Test getattribute in an expression
command += oiiotool ('-echo "Expr getattribute(\"limits:channels\") = {getattribute(\"limits:channels\")}"')

# Test --if --else --endif
command += oiiotool ('-echo "Testing if with true cond (expect output):" -set i 42 -if "{i}" -echo "  inside if clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing if with false cond (expect NO output):" -set i 0 -if "{i}" -echo "  inside if clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing if/else with true cond:" -set i 42 -if "{i}" -echo "  inside if clause, i={i}" -else -echo "  inside else clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing if/else with false cond:" -set i 0 -if "{i}" -echo "  inside if clause, i={i}" -else -echo "  inside else clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing else without if:" -else -echo "bad" -endif -echo " "')
command += oiiotool ('-echo "Testing endif without if:" -endif -echo " "')

# Test --while --endwhile
command += oiiotool ('-echo "Testing while (expect output 0..2):" -set i 0 --while "{i < 3}" --echo "  i = {i}" --set i "{i+1}" --endwhile -echo " "')
command += oiiotool ('-echo "Testing endwhile without while:" -endwhile -echo " "')

# Test --for --endfor
command += oiiotool (
      '-echo "Testing for i 5 (expect output 0..4):" --for i 5 --echo "  i = {i}" --endfor -echo " " '
    + '-echo "Testing for i 5,10 (expect output 5..9):" --for i 5,10 --echo "  i = {i}" --endfor -echo " " '
    + '-echo "Testing for i 5,10,2 (expect output 5,7,9):" --for i 5,10,2 --echo "  i = {i}" --endfor -echo " " '
    + '-echo "Testing for i 10,5,-1 (expect output 10..6):" --for i 10,5,-1 --echo "  i = {i}" --endfor -echo " " '
    + '-echo "Testing for i 10,5 (expect output 10..6):" --for i 10,5 --echo "  i = {i}" --endfor -echo " " '
    )
command += oiiotool ('-echo "Testing endfor without for:" -endfor -echo " "')
command += oiiotool ('-echo "Testing for i 5,10,2,8 (bad range):" --for i 5,10,2,8 --echo "  i = {i}" --endfor -echo " "')


# test sequences
command += oiiotool ("../common/tahoe-tiny.tif -o copyA.1-10#.jpg")
command += oiiotool ("--debug copyA.#.jpg -o copyB.#.jpg")
command += oiiotool (" --info  " +  " ".join(["copyA.{0:04}.jpg".format(x) for x in range(1,11)]))
command += oiiotool ("--frames 1-5 --echo \"Sequence 1-5:  {FRAME_NUMBER}\"")
command += oiiotool ("--frames -5-5 --echo \"Sequence -5-5:  {FRAME_NUMBER}\"")
command += oiiotool ("--frames -5--2 --echo \"Sequence -5--2:  {FRAME_NUMBER}\"")

# Sequence errors:
# No matching files found
command += oiiotool ("notfound.#.jpg -o alsonotfound.#.jpg")
# Ranges don't match
command += oiiotool ("copyA.#.jpg -o copyC.1-5#.jpg")

# Test stats and metadata expression substitution
command += oiiotool ("../common/tahoe-tiny.tif"
                     + " --echo \"\\nBrief: {TOP.METABRIEF}\""
                     + " --echo \"\\nBrief native: {TOP.METANATIVEBRIEF}\""
                     + " --echo \"\\nMeta native: {TOP.METANATIVE}\""
                     + " --echo \"\\nStats:\\n{TOP.STATS}\\n\"")

# Test IMG[], TOP, BOTTOM
command += oiiotool ("../common/tahoe-tiny.tif ../common/tahoe-small.tif ../common/grid.tif " +
                     "--echo \"Stack holds [0] = {IMG[0].filename}, [1] = {IMG[1].filename}, [2] = {IMG[2].filename}\" " +
                     "--echo \"TOP = {TOP.filename}, BOTTOM = {BOTTOM.filename}\" " +
                     "--set i 1 " +
                     "--echo \"Stack holds [{i}] = {IMG[i].filename}\" "
                     )

# Test some special attribute evaluation names
command += oiiotool ("../common/tahoe-tiny.tif " +
                     "--echo \"filename={TOP.filename} file_extension={TOP.file_extension} file_noextension={TOP.file_noextension}\" " +
                     "--echo \"MINCOLOR={TOP.MINCOLOR} MAXCOLOR={TOP.MAXCOLOR} AVGCOLOR={TOP.AVGCOLOR}\"")

command += oiiotool ("--echo \"Testing expressions IS_BLACK, IS_CONSTANT:\" " +
                     "--pattern:type=uint16 constant:color=0.5,0.5,0.5 4x4 3 " +
                     "--echo \"  grey is-black? {TOP.IS_BLACK} is-constant? {TOP.IS_CONSTANT}\" " +
                     "--pattern:type=uint16 constant:color=0,0,0 4x4 3 " +
                     "--echo \"  black is-black? {TOP.IS_BLACK} is-constant? {TOP.IS_CONSTANT}\" " +
                     "--pattern:type=uint16 fill:left=0,0,0:right=1,1,1 4x4 3 " +
                     "--echo \"  gradient is-black? {TOP.IS_BLACK} is-constant? {TOP.IS_CONSTANT}\" "
                    )
command += oiiotool (
    "--echo \"Testing NIMAGES:\" " +
    "--echo \"  {NIMAGES}\" " +
    "--create 16x16 3 " +
    "--echo \"  {NIMAGES}\" " +
    "--create 16x16 3 " +
    "--echo \"  {NIMAGES}\" " +
    "--create 16x16 3 " +
    "--echo \"  {NIMAGES}\" ")

# Test "postpone_callback" with an "image -BINOP image" operation instead of
# "image image -BINOP".
command += oiiotool ("../common/tahoe-tiny.tif -sub ../common/tahoe-tiny.tif "
                     + "-echo \"postponed sub:\" --printinfo:stats=1:verbose=0")

# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "exprgradient.tif", "exprcropped.tif", "exprstrcatlzw.tif",
            "tahoe-contraststretch.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
