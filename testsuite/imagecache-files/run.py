#!/usr/bin/python 

import os
import sys
import shutil

path = ""
command = ""
if len(sys.argv) > 2 :
    os.chdir (sys.argv[1])
    path = sys.argv[2] + "/"

sys.path = [".."] + sys.path
import runtest

# Files that need to be cleaned up, IN ADDITION to outputs
cleanfiles = [ "out.txt" "out.exr" ]

texfile = "../../../oiio-images/grid.tx"

# Make 10 copies of the grid texture, to different names to force
# lots of filse in the cache.
allnames = ""
for i in range(10) :
    name = "f%02d.tx" % i
    shutil.copy (texfile, name)
    allnames = allnames + " " + name
    cleanfiles.append (name)

# Run testtex with small cache size and max files.  We will check its
# output to make sure we hit the right peak levels.
command = (path + runtest.oiio_app("testtex") +
           " --cachesize 10 --maxfiles 5 --blocksize 16 " + allnames +
           " | grep peak > out.txt")
#           " | grep -v time | grep -v Tot > out.txt")
# N.B. we use grep to exclude time values that may differ from run to run

# Outputs to check against references
outputs = [ "out.txt" ]


# boilerplate
ret = runtest.runtest (command, outputs, cleanfiles)
sys.exit (ret)
