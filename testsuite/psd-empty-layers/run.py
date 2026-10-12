#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '
command += run_app(f'"{pythonbin}" src/make-empty-layer-psds.py')
for compression in (0, 1):
    for width, height in ((0, 0), (0, 2), (2, 0)):
        filename = f"empty-layer-{width}x{height}-{compression}.psd"
        command += info_command(filename, verbose=False, extraargs="-a")

for filename in ("hidden-empty-layer-0.psd", "hidden-empty-layer-1.psd",
                 "hidden-populated-layer.psd"):
    command += info_command(filename, verbose=False, extraargs="-a")

command += info_command("empty-layer-invalid-canvas.psd", failureok=True)
command += info_command("empty-layer-oversized.psd", failureok=True)
