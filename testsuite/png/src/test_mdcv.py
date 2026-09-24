#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0

import binascii
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

try:
    import OpenImageIO as oiio
except ModuleNotFoundError as error:
    if error.name != "OpenImageIO":
        raise
    oiio = None


NAMES = tuple(
    "mdcv_" + name
    for name in (
        "red_x", "red_y", "green_x", "green_y", "blue_x", "blue_y",
        "white_x", "white_y", "max_luminance", "min_luminance"
    )
)
VALUES = (34000, 16000, 13250, 34500, 7500, 3000, 15635, 16450,
          10000000, 0)
CICP = bytes((9, 16, 0, 1))


def chunk(kind, payload):
    checksum = binascii.crc32(kind + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def png(path, with_cicp=False, with_mdcv=False, values=VALUES):
    chunks = [chunk(b"IHDR", struct.pack(">2I5B", 1, 1, 8, 2, 0, 0, 0))]
    if with_cicp:
        chunks.append(chunk(b"cICP", CICP))
    if with_mdcv:
        chunks.append(chunk(b"mDCV", struct.pack(">8H2I", *values)))
    chunks.extend((chunk(b"IDAT", zlib.compress(bytes((0, 64, 96, 128)))),
                   chunk(b"IEND", b"")))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + b"".join(chunks))


def read_chunks(path):
    data = path.read_bytes()
    # A PNG-encoded ICO icon is a whole PNG stream after the icon directory.
    start = data.index(b"\x89PNG\r\n\x1a\n")
    assert start == 0 or path.suffix == ".ico"
    result = {}
    order = []
    offset = start + 8
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + size]
        checksum = struct.unpack_from(">I", data, offset + 8 + size)[0]
        assert checksum == binascii.crc32(kind + payload) & 0xFFFFFFFF
        result[kind] = payload
        order.append(kind)
        offset += size + 12
    return result, order


def assert_mdcv_order(order):
    assert order.index(b"mDCV") < order.index(b"IDAT")
    if b"PLTE" in order:
        assert order.index(b"mDCV") < order.index(b"PLTE")


def invoke(tool, *args):
    # Omitted mDCV records are reported on OIIO's debug channel (stderr).
    env = dict(os.environ, OPENIMAGEIO_DEBUG="1")
    env.pop("OPENIMAGEIO_DEBUG_FILE", None)
    return subprocess.run((tool, *args), text=True, capture_output=True,
                          env=env)


def assert_omitted(result, path, reason):
    # An unwritable record never fails the write: it is reported and omitted.
    assert result.returncode == 0, result.stderr
    assert "OpenImageIO WARNING: " in result.stderr, result.stderr
    assert reason + "; omitting the mDCV chunk" in result.stderr, result.stderr
    assert b"mDCV" not in read_chunks(path)[0]


def read_record(tool, path):
    # An unknown attribute is an error for --echo, and a record can be absent
    # (older libpng, or one libpng itself rejects), so read the whole record
    # from --info, where a missing attribute is simply a missing line.
    result = invoke(tool, "--info", "-v", str(path))
    assert result.returncode == 0, result.stderr
    found = {}
    for line in result.stdout.splitlines():
        name, separator, value = line.strip().partition(": ")
        if separator and name in NAMES:
            found[name] = int(value)
    # A partially exposed record is invalid on either capability path.
    assert len(found) in (0, len(NAMES)), result.stdout
    return tuple(found[name] for name in NAMES) if found else None


def attributes(values=VALUES):
    result = []
    for name, value in zip(NAMES, values):
        result.extend(("--attrib:type=int", name, str(value)))
    return result


tool = sys.argv[1]
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source = root / "source.png"
    plain = root / "plain.png"
    output = root / "output.png"
    png(source, with_cicp=True, with_mdcv=True)
    png(plain)

    # Input support is independent of the writer's additional cICP gate. When
    # bindings are built, verify the input plugin's advertised capability.
    input_supported = None
    output_supported = None
    if oiio is not None:
        png_input = oiio.ImageInput.open(str(source))
        assert png_input
        input_supported = bool(png_input.supports("mdcv"))
        png_input.close()
        output_supported = bool(oiio.ImageOutput.create("png").supports("mdcv"))
    record = read_record(tool, source)
    if input_supported is not None:
        assert (record is not None) == input_supported, record
    if record is not None:
        assert record == VALUES, record

    write = invoke(tool, str(source), *attributes(), "--cicp", "9,16,0,1",
                   "-o", str(output))
    unsupported = "PNG output does not support mDCV metadata"
    if output_supported is not None:
        assert output_supported == (unsupported not in write.stderr)
    if unsupported in write.stderr:
        assert_omitted(write, output, unsupported)
    else:
        assert write.returncode == 0, write.stderr
        parsed, order = read_chunks(output)
        assert parsed[b"cICP"] == CICP
        assert parsed[b"mDCV"] == struct.pack(">8H2I", *VALUES)
        assert_mdcv_order(order)

        # Copy-through is meaningful only when input exposed the whole record.
        if record is not None:
            copied_path = root / "copied.png"
            copied_record = invoke(tool, str(source), "-o", str(copied_path))
            assert copied_record.returncode == 0, copied_record.stderr
            copied_chunks, copied_order = read_chunks(copied_path)
            assert copied_chunks[b"mDCV"] == struct.pack(">8H2I", *VALUES)
            assert_mdcv_order(copied_order)

            # Input does not apply the output limits, so a copy of an
            # out-of-range record reads it but omits the chunk.
            wide = VALUES[:8] + (200000000, 0)
            wide_source = root / "wide.png"
            png(wide_source, with_cicp=True, with_mdcv=True, values=wide)
            assert read_record(tool, wide_source) == wide
            wide_copy = root / "wide_copy.png"
            result = invoke(tool, str(wide_source), "-o", str(wide_copy))
            assert_omitted(result, wide_copy,
                           "maximum luminance must be in [1, 100000000]")

            # libpng drops a record whose luminance exceeds its own
            # 0x7fffffff limit, so no out-of-int value is ever reported.
            huge_source = root / "huge.png"
            png(huge_source, with_cicp=True, with_mdcv=True,
                values=VALUES[:8] + (0x80000000, 0))
            assert read_record(tool, huge_source) is None

        pixels = invoke(tool, str(source), str(output), "--diff")
        assert pixels.returncode == 0, pixels.stdout + pixels.stderr

        absent = root / "absent.png"
        copied = invoke(tool, str(plain), "-o", str(absent))
        assert copied.returncode == 0, copied.stderr
        absent_chunks, _ = read_chunks(absent)
        assert b"mDCV" not in absent_chunks

        partial = invoke(tool, str(plain), "--cicp", "9,16,0,1",
                         "--attrib:type=int", NAMES[0], str(VALUES[0]),
                         "-o", str(root / "partial.png"))
        assert_omitted(partial, root / "partial.png",
                       "requires all ten mdcv_* attributes")

        malformed = invoke(tool, str(plain), "--cicp", "9,16,0,1",
                           *attributes(), "--attrib:type=string", NAMES[0], "bad",
                           "-o", str(root / "malformed.png"))
        assert_omitted(malformed, root / "malformed.png",
                       "must be a scalar integer")

        for chromaticity in (50001, -1):
            invalid = invoke(tool, str(plain), "--cicp", "9,16,0,1",
                             *attributes((chromaticity,) + VALUES[1:]),
                             "-o", str(root / "invalid.png"))
            assert_omitted(invalid, root / "invalid.png",
                           "must be in [0, 50000]")

        uncoupled = invoke(tool, str(plain), *attributes(),
                           "-o", str(root / "uncoupled.png"))
        assert_omitted(uncoupled, root / "uncoupled.png",
                       "requires an accompanying cICP chunk")

        # Luminance range and ordering; each record fails only one check.
        maximum = "maximum luminance must be in [1, 100000000]"
        minimum = "minimum luminance must be in [0, 99999999]"
        for luminance, reason in (
                ((0, 0), maximum),
                ((100000001, 0), maximum),
                ((10000000, -1), minimum),
                ((100000000, 100000000), minimum),
                ((10000000, 10000000), "must be less than maximum luminance")):
            path = root / "luminance.png"
            result = invoke(tool, str(plain), "--cicp", "9,16,0,1",
                            *attributes(VALUES[:8] + luminance), "-o", str(path))
            assert_omitted(result, path, reason)

        # The inclusive limits of every range are written.
        edges = (0, 50000) * 4 + (100000000, 99999999)
        path = root / "edges.png"
        result = invoke(tool, str(plain), "--cicp", "9,16,0,1",
                        *attributes(edges), "-o", str(path))
        assert result.returncode == 0, result.stderr
        assert "OpenImageIO WARNING: " not in result.stderr, result.stderr
        assert read_chunks(path)[0][b"mDCV"] == struct.pack(">8H2I", *edges)

        # Unsigned integer attributes are accepted too (oiiotool writes int).
        if oiio is not None:
            spec = oiio.ImageSpec(1, 1, 3, "uint8")
            spec.attribute("CICP", "int[4]", tuple(CICP))
            for name, value in zip(NAMES, VALUES):
                spec.attribute(name, "uint", value)
            path = root / "uint.png"
            assert oiio.ImageBuf(spec).write(str(path))
            assert read_chunks(path)[0][b"mDCV"] == struct.pack(">8H2I", *VALUES)

        # ICO reads and writes PNG-encoded icons through the same code.
        if " ico : " in invoke(tool, "--list-formats").stdout:
            # One 1x1 icon: ICONDIR, one ICONDIRENTRY, then the PNG stream.
            icon = root / "icon.ico"
            stream = source.read_bytes()
            icon.write_bytes(struct.pack("<3H4B2H2I", 0, 1, 1, 1, 1, 0, 0, 1,
                                         32, len(stream), 22) + stream)
            assert read_record(tool, icon) == VALUES

            icon_output = root / "output.ico"
            result = invoke(tool, str(icon), "--attrib:type=int", "ico:PNG",
                            "1", "-o", str(icon_output))
            assert result.returncode == 0, result.stderr
            icon_chunks = read_chunks(icon_output)[0]
            assert icon_chunks[b"cICP"] == CICP
            assert icon_chunks[b"mDCV"] == struct.pack(">8H2I", *VALUES)

            partial_icon = root / "partial.ico"
            result = invoke(tool, str(plain), "--attrib:type=int", "ico:PNG",
                            "1", "--cicp", "9,16,0,1", "--attrib:type=int",
                            NAMES[0], str(VALUES[0]), "-o", str(partial_icon))
            assert_omitted(result, partial_icon,
                           "requires all ten mdcv_* attributes")
