import invoke
import logging
from pathlib import Path
import os
import shutil
import tempfile
import zipfile
logger = logging.getLogger(__name__)


def _wheel_edit(wheel_path):
    """
    Edit the contents of the wheel to remove the 'lib', 'share', and 'include' directories,
    and ensure all files under 'OpenImageIO/bin' are executable.
    """
    dirs_to_remove = ['lib', 'lib64', 'share', 'include']

    with tempfile.TemporaryDirectory() as tempdir:
        # Unzip the wheel into the temporary directory
        with zipfile.ZipFile(wheel_path, 'r') as zip_ref:
            zip_ref.extractall(tempdir)

        # Remove the specified directories
        for root, dirs, files in os.walk(tempdir):
            for dir_name in dirs_to_remove:
                dir_path = Path(root) / 'OpenImageIO' / dir_name
                if dir_path.is_dir():
                    logger.debug(f"Removing {dir_path}")
                    shutil.rmtree(dir_path)

        # Remove the RECORD file entries for the specified directories
        record_file_path = None
        for root, dirs, files in os.walk(tempdir):
            for dir in dirs:
                if dir.endswith(".dist-info"):
                    possible_record_file = Path(root) / dir / 'RECORD'
                    if possible_record_file.is_file():
                        record_file_path = possible_record_file
                        break
            if record_file_path:
                break

        if record_file_path:
            with open(record_file_path, 'r') as record_file:
                lines = record_file.readlines()

            new_lines = []
            remove_prefixes = [f"OpenImageIO/{lib}" for lib in dirs_to_remove]

            for line in lines:
                if not any(line.startswith(prefix) for prefix in remove_prefixes):
                    new_lines.append(line)

            with open(record_file_path, 'w') as record_file:
                record_file.writelines(new_lines)

        # Ensure all files under 'OpenImageIO/bin' are executable
        bin_dir = Path(tempdir) / 'OpenImageIO' / 'bin'
        if bin_dir.exists():
            for file_path in bin_dir.iterdir():
                if file_path.is_file():
                    logger.debug(f"Making {file_path} executable")
                    file_path.chmod(file_path.stat().st_mode | 0o111)

        # Create a new wheel file with the modifications
        with zipfile.ZipFile(wheel_path, 'w') as zip_ref:
            for root, dirs, files in os.walk(tempdir):
                for file in files:
                    file_path = Path(root) / file
                    arcname = file_path.relative_to(tempdir)
                    zip_ref.write(file_path, arcname)

        logger.debug(f"Modified wheel created at {wheel_path}")

        return Path(wheel_path)
    

def _wheel_edit_alt(wheel):
    """
    Edit the contents of the repaired wheel to remove the 'lib', 'share', and 'include' directories.
    """
    from repairwheel._vendor.auditwheel.wheeltools import InWheelCtx

    dirs_to_remove = ['share', 'lib', 'lib64', 'include']

    with InWheelCtx(wheel, wheel) as ctx:
        root = Path(ctx.path)
        for dir_name in dirs_to_remove:
            this_dir = root/"OpenImageIO"/dir_name
            if this_dir.exists():
                shutil.rmtree(this_dir)
    
    return Path(wheel)


@invoke.task
def wheel_repair(c, build_dir, wheel_path, output_dir):
    """
    Slim down and repair the wheel file at `wheel_path` with libraries from `build_dir` and save the result to `output_dir`.
    """
    edited_wheel_path = _wheel_edit(wheel_path)
    c.run(f"repairwheel -l {build_dir}/deps/dist/lib -l {build_dir}/lib -o {output_dir} {edited_wheel_path}")
    print(f"Repaired + slimmed wheel created at {output_dir}")
