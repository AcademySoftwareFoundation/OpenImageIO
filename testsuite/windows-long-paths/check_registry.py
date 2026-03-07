"""Helper script to detect system-level long path support on Windows.

Returns 0 if the long path support is enabled in the registry, or 1 otherwise.

Reference: https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry#registry-setting-to-enable-long-paths
"""

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import sys
import winreg

_SUB_KEY = "SYSTEM\\CurrentControlSet\\Control\\FileSystem"
_VALUE_NAME = "LongPathsEnabled"


def main() -> int:
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, _SUB_KEY) as key:
            reg_value, value_type = winreg.QueryValueEx(key, _VALUE_NAME)
    except OSError:
        # Key does not exist
        return 1

    # It's vanishingly unlikely that someone would stuff some other value type
    # in this key, but let's be paranoid.
    if value_type != winreg.REG_DWORD:
        return 1

    return int(reg_value != 1)


if __name__ == "__main__":
    sys.exit(main())
