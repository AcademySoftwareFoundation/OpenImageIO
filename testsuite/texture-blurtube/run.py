#!/usr/bin/env python 

# This test renders the "tube" pattern with a checker pattern (i.e.,
# showing all angles and a wide range of anisotropy), at several
# different blur levels.

command += testtex_command ("../common/textures/grid.tx",
                            "--tube --blur 0.0 -d uint8 -o grid-0.00.tif")
command += testtex_command ("../common/textures/grid.tx",
                            "--tube --blur 0.02 -d uint8 -o grid-0.02.tif")
command += testtex_command ("../common/textures/grid.tx",
                            "--tube --blur 0.05 -d uint8 -o grid-0.05.tif")
command += testtex_command ("../common/textures/grid.tx",
                            "--tube --blur 0.1 -d uint8 -o grid-0.10.tif")
command += testtex_command ("../common/textures/grid.tx",
                            "--tube --blur 0.2 -d uint8 -o grid-0.20.tif")
outputs = [ "grid-0.00.tif", "grid-0.02.tif", "grid-0.05.tif",
            "grid-0.10.tif", "grid-0.20.tif" ]


command += testtex_command ("../common/textures/checker.tx",
                            "--tube --blur 0.0 -d uint8 -o checker-0.00.tif")
command += testtex_command ("../common/textures/checker.tx",
                            "--tube --blur 0.02 -d uint8 -o checker-0.02.tif")
command += testtex_command ("../common/textures/checker.tx",
                            "--tube --blur 0.05 -d uint8 -o checker-0.05.tif")
command += testtex_command ("../common/textures/checker.tx",
                            "--tube --blur 0.1 -d uint8 -o checker-0.10.tif")
command += testtex_command ("../common/textures/checker.tx",
                            "--tube --blur 0.2 -d uint8 -o checker-0.20.tif")
outputs += [ "checker-0.00.tif", "checker-0.02.tif", "checker-0.05.tif",
             "checker-0.10.tif", "checker-0.20.tif" ]
