#!/usr/bin/python 

import sys
sys.path = ["..", "testsuite"] + sys.path
import runtest

# A command to run
command = runtest.oiio_app("testtex") + " --nowarp --offset -1 -1 -1 --scalest 2 2 " + os.path.relpath("sparse_half.f3d",runtest.tmpdir)

outputs = [ "out.exr" ]

# boilerplate
ret = runtest.runtest (command, outputs)
sys.exit (ret)
