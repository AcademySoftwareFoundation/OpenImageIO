#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Spec 09 Feature 1 -- writer-boundary leak check.
#
# Scans a written file's RAW BYTES for an authored colorInteropID. This is
# deliberately NOT a read-back check: a slotless format can embed the id in a
# container field its own reader never maps back to a `colorInteropID`
# attribute (PNG's tEXt is exactly this case), so a round-trip assertion
# passes while the file still leaks. The policy contract is about what reaches
# the FILE, so the file is what we inspect.
#
# Prints one deterministic line per file: the id token must not appear when
# the format has no native colorInteropID slot and the policy says not to
# force one.

from __future__ import print_function
import sys


def main(argv):
    # argv: <token> <file> [<file> ...]
    token = argv[1].encode("utf-8")
    for path in argv[2:]:
        try:
            with open(path, "rb") as f:
                data = f.read()
        except IOError:
            print("{}: MISSING".format(path))
            continue
        # Report the token, not the file's whole payload -- keeps the
        # reference output stable across libpng/libtiff versions.
        found = token in data
        print("{}: colorInteropID in file bytes = {}".format(path, found))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
