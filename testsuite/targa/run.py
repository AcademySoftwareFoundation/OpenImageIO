#!/usr/bin/env python

redirect = " >> out.txt 2>> out.err.txt"

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/targa"

files = [ "CBW8.TGA", "CCM8.TGA", "CTC16.TGA", "CTC24.TGA", "CTC32.TGA",
          "UBW8.TGA", "UCM8.TGA", "UTC16.TGA", "UTC24.TGA", "UTC32.TGA",
          "round_grill.tga" ]
for f in files:
    command += rw_command (imagedir, f)


# Test corrupted files
command += iconvert("-v src/crash1.tga crash1.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash2.tga -o crash2.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash3.tga -o crash3.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash4.tga -o crash4.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash5.tga -o crash5.exr", failureok = True)
command += oiiotool("--oiioattrib try_all_readers 0 src/crash6.tga -o crash6.exr", failureok = True)

# Test odds and ends, unusual files
command += rw_command("src", "1x1.tga")

outputs += [ 'out.txt', 'out.err.txt' ]
