# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import invoke
import logging
from pathlib import Path
import os
import shutil
import tempfile
import zipfile

logger = logging.getLogger(__name__)

DIRS_TO_REMOVE = ['lib', 'lib64', 'share', 'include']

def _wheel_edit(wheel_path, dirs_to_remove=DIRS_TO_REMOVE):
    """
    Edit the contents of the repaired wheel to remove the 'lib', 'share', and 'include' directories.
    """
    from repairwheel._vendor.auditwheel.wheeltools import InWheelCtx

    with InWheelCtx(wheel_path, wheel_path) as ctx:
        root = Path(ctx.path)
        for dir_name in dirs_to_remove:
            this_dir = root / "OpenImageIO" / dir_name
            if this_dir.exists():
                shutil.rmtree(this_dir)
    
    return Path(wheel_path)


@invoke.task
def wheel_repair(c, build_dir, wheel_path, output_dir):
    """
    Slim down and repair the wheel file at `wheel_path` with libraries from `build_dir` and save the result to `output_dir`.
    """
    edited_wheel_path = _wheel_edit(wheel_path)
    c.run(f"repairwheel -l {build_dir}/deps/dist/lib -l {build_dir}/lib -o {output_dir} {edited_wheel_path}")
    print(f"Repaired + slimmed wheel created at {output_dir}")
