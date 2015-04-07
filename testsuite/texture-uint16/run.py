#!/usr/bin/env python 

command += maketx_command  ("../common/textures/grid.tx", "grid-uint16.tx",
                            "-d uint16", showinfo=True)
command += testtex_command ("grid-uint16.tx")
outputs = [ "out.exr" ]
