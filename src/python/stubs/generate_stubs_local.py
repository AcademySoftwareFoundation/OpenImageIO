"""
Script that is called when buildling the stubs during local development, via
`make pystubs` or `cmake --target pystubs`.
"""
import platform

import argparse
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root")
    parser.add_argument("--output-dir")
    parser.add_argument("--python-version")
    parser.add_argument("--cibuildwheel-version")

    args = parser.parse_args()

    version_major, version_minor = args.python_version.split(".")

    # detect the platform so that we can avoid running docker under emulation.
    if platform.system() == "Darwin":
        # On Mac, `uname -m`,  $CMAKE_HOST_SYSTEM_PROCESSOR, and platform.machine() returns
        # x86_64 even on arm64 processors (i.e. Apple Silicon) if cmake or python is running under
        # emulation (Rosetta). sysctl.proc_translated tells us if we're under emulation.
        arch = subprocess.check_output(["uname", "-m"], text=True).strip()
        if arch == "x86_64":
            process_uses_emulation = subprocess.check_output(
                ["sysctl", "-in", "sysctl.proc_translated"], text=True
            ).strip()
            if process_uses_emulation == "1":
                arch = "arm64"
    else:
        arch = platform.machine()
    if arch == "arm64":
        arch = "aarch64"
    python_build_id = f"cp{version_major}{version_minor}-manylinux_{arch}"
    print(f"Building {python_build_id}")

    try:
        subprocess.check_call(
            [
                "uv",
                "tool",
                "run",
                f"--python={sys.executable}",
                f"cibuildwheel@{args.cibuildwheel_version}",
                f"--output-dir={args.output_dir}",
                f"--only={python_build_id}",
                ".",
            ],
            cwd=args.repo_root,
        )
    except FileNotFoundError:
        print("\nERROR: You must install uv to build the stubs. See https://docs.astral.sh/uv/\n")
        sys.exit(1)


if __name__ == "__main__":
    main()
