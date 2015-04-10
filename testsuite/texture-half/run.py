#!/usr/bin/env python 

command += maketx_command  ("../common/textures/grid.tx", "grid-half.exr",
                            "-d half", showinfo=True)
command += testtex_command ("grid-half.exr")
outputs = [ "out.exr" ]
