#!/usr/bin/env python

# Create some test images we need
# command += oiiotool ("--create 320x240 3 -d uint8 -o black.tif")
# command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 128x128 3 -d half -o grey128.exr")
# command += oiiotool ("--pattern constant:color=0.5,0.5,0.5 64x64 3 -d half -o grey64.exr")
# command += oiiotool ("--create 256x256 3 --fill:color=1,.5,.5 256x256 --fill:color=0,1,0 80x80+100+100 -d uint8 -o filled.tif")


# test expression substitution
command += oiiotool ('-echo "42+2 = {42+2}"')
command += oiiotool ('-echo "42-2 = {42-2}"')
command += oiiotool ('-echo "42*2 = {42*2}"')
command += oiiotool ('-echo "42/2 = {42/2}"')

command += oiiotool ('-echo "42<41 = {42<41}"')
command += oiiotool ('-echo "42<42 = {42<42}"')
command += oiiotool ('-echo "42<43 = {42<43}"')
command += oiiotool ('-echo "42<=41 = {42<=41}"')
command += oiiotool ('-echo "42<=42 = {42<=42}"')
command += oiiotool ('-echo "42<=43 = {42<=43}"')
command += oiiotool ('-echo "42>41 = {42>41}"')
command += oiiotool ('-echo "42>42 = {42>42}"')
command += oiiotool ('-echo "42>43 = {42>43}"')
command += oiiotool ('-echo "42>=41 = {42>=41}"')
command += oiiotool ('-echo "42>=42 = {42>=42}"')
command += oiiotool ('-echo "42>=43 = {42>=43}"')
command += oiiotool ('-echo "42==41 = {42==41}"')
command += oiiotool ('-echo "42==42 = {42==42}"')
command += oiiotool ('-echo "42==43 = {42==43}"')
command += oiiotool ('-echo "42!=41 = {42!=41}"')
command += oiiotool ('-echo "42!=42 = {42!=42}"')
command += oiiotool ('-echo "42!=43 = {42!=43}"')
command += oiiotool ('-echo "42<=>41 = {42<=>41}"')
command += oiiotool ('-echo "42<=>42 = {42<=>42}"')
command += oiiotool ('-echo "42<=>43 = {42<=>43}"')

command += oiiotool ('-echo "(1==2)&&(2==2) = {(1==2)&&(2==2)}"')
command += oiiotool ('-echo "(1==1)&&(2==2) = {(1==1)&&(2==2)}"')
command += oiiotool ('-echo "(1==2)&&(1==2) = {(1==2)&&(1==2)}"')
command += oiiotool ('-echo "(1==2)||(2==2) = {(1==2)||(2==2)}"')
command += oiiotool ('-echo "(1==1)||(2==2) = {(1==1)||(2==2)}"')
command += oiiotool ('-echo "(1==2)||(1==2) = {(1==2)||(1==2)}"')
command += oiiotool ('-echo "not(1==1) = {not(1==1)}"')
command += oiiotool ('-echo "not(1==2) = {not(1==2)}"')
command += oiiotool ('-echo "!(1==1) = {!(1==1)}"')
command += oiiotool ('-echo "!(1==2) = {!(1==2)}"')

command += oiiotool ('-echo "eq(foo,foo) = {eq(\'foo\',\'foo\')}"')
command += oiiotool ('-echo "eq(foo,bar) = {eq(\'foo\',\'bar\')}"')
command += oiiotool ('-echo "neq(foo,foo) = {neq(\'foo\',\'foo\')}"')
command += oiiotool ('-echo "neq(foo,bar) = {neq(\'foo\',\'bar\')}"')

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
command += oiiotool ('-echo "Testing --set:" -set i 1 -echo "  i = {i}" -set i "{i+41}" -echo "  now i = {i}"')

# Test --if --else --endif
command += oiiotool ('-echo "Testing if with true cond (expect output):" -set i 42 -if "{i}" -echo "  inside if clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing if with false cond (expect NO output):" -set i 0 -if "{i}" -echo "  inside if clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing if/else with true cond:" -set i 42 -if "{i}" -echo "  inside if clause, i={i}" -else -echo "  inside else clause, i={i}" -endif -echo "  done" -echo " "')
command += oiiotool ('-echo "Testing if/else with false cond:" -set i 0 -if "{i}" -echo "  inside if clause, i={i}" -else -echo "  inside else clause, i={i}" -endif -echo "  done" -echo " "')

# Test --while --endwhile
command += oiiotool ('-echo "Testing while (expect output 0..2):" -set i 0 --while "{i < 3}" --echo "  i = {i}" --set i "{i+1}" --endwhile -echo " "')

# Test --for --endfor
command += oiiotool ('-echo "Testing for i 5 (expect output 0..4):" --for i 5 --echo "  i = {i}" --endfor -echo " "')
command += oiiotool ('-echo "Testing for i 5,10 (expect output 5..9):" --for i 5,10 --echo "  i = {i}" --endfor -echo " "')
command += oiiotool ('-echo "Testing for i 5,10,2 (expect output 5,7,9):" --for i 5,10,2 --echo "  i = {i}" --endfor -echo " "')

# test sequences
command += oiiotool ("../common/tahoe-tiny.tif -o copyA.1-10#.jpg")
command += oiiotool (" --info  " +  " ".join(["copyA.{0:04}.jpg".format(x) for x in range(1,11)]))
command += oiiotool ("--frames 1-5 --echo \"Sequence 1-5:  {FRAME_NUMBER}\"")
command += oiiotool ("--frames -5-5 --echo \"Sequence -5-5:  {FRAME_NUMBER}\"")
command += oiiotool ("--frames -5--2 --echo \"Sequence -5--2:  {FRAME_NUMBER}\"")

# Test stats and metadata expression substitution
command += oiiotool ("../common/tahoe-tiny.tif --echo \"\\nBrief: {TOP.METABRIEF}\"")
command += oiiotool ("../common/tahoe-tiny.tif --echo \"\\nMeta: {TOP.META}\"")
command += oiiotool ("../common/tahoe-tiny.tif --echo \"\\nStats:\\n{TOP.STATS}\\n\"")



# To add more tests, just append more lines like the above and also add
# the new 'feature.tif' (or whatever you call it) to the outputs list,
# below.


# Outputs to check against references
outputs = [
            "exprgradient.tif", "exprcropped.tif", "exprstrcatlzw.tif",
            "tahoe-contraststretch.tif",
            "out.txt" ]

#print "Running this command:\n" + command + "\n"
