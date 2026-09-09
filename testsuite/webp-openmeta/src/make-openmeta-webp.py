#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import struct


BASE = "base.webp"
TIFF_OUTPUT = "openmeta-metadata.webp"
PREFIXED_OUTPUT = "openmeta-prefixed-metadata.webp"
EXIF_FLAG = 0x08
XMP_FLAG = 0x04


def read_chunks(data):
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WEBP":
        raise RuntimeError("base image is not a RIFF WebP file")

    chunks = []
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise RuntimeError("truncated WebP chunk header")
        fourcc = data[offset : offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        begin = offset + 8
        end = begin + size
        if end > len(data):
            raise RuntimeError("truncated WebP chunk payload")
        chunks.append((fourcc, data[begin:end]))
        offset = end + (size & 1)
    return chunks


def write_chunk(fourcc, payload):
    chunk = fourcc + struct.pack("<I", len(payload)) + payload
    if len(payload) & 1:
        chunk += b"\x00"
    return chunk


def make_tiff():
    make = b"OpenMetaCamera\x00"
    ifd0_size = 2 + 3 * 12 + 4
    make_offset = 8 + ifd0_size
    padded_make_size = (len(make) + 1) & ~1
    exif_ifd_offset = make_offset + padded_make_size
    exif_ifd_size = 2 + 12 + 4
    aperture_offset = exif_ifd_offset + exif_ifd_size
    tiff = bytearray(b"II")
    tiff += struct.pack("<H", 42)
    tiff += struct.pack("<I", 8)
    tiff += struct.pack("<H", 3)
    tiff += struct.pack("<HHII", 0x010F, 2, len(make), make_offset)
    tiff += struct.pack("<HHI", 0x0112, 3, 1)
    tiff += struct.pack("<H", 6) + b"\x00\x00"
    tiff += struct.pack("<HHII", 0x8769, 4, 1, exif_ifd_offset)
    tiff += struct.pack("<I", 0)
    tiff += make + b"\x00" * (padded_make_size - len(make))
    tiff += struct.pack("<H", 1)
    tiff += struct.pack("<HHII", 0x9202, 5, 1, aperture_offset)
    tiff += struct.pack("<I", 0)
    tiff += struct.pack("<II", 12, 5)
    return bytes(tiff)


def make_xmp():
    return (
        b"<x:xmpmeta xmlns:x='adobe:ns:meta/'>"
        b"<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
        b"<rdf:Description xmlns:xmp='http://ns.adobe.com/xap/1.0/' "
        b"xmlns:exif='http://ns.adobe.com/exif/1.0/' "
        b"xmlns:xmpMM='http://ns.adobe.com/xap/1.0/mm/' "
        b"xmlns:stEvt='http://ns.adobe.com/xap/1.0/sType/ResourceEvent#' "
        b"xmp:CreatorTool='OpenMeta WebP pilot' xmp:Rating='4' "
        b"exif:ApertureValue='99/10' xmpMM:DocumentID='test:document'>"
        b"<xmpMM:History><rdf:Seq><rdf:li rdf:parseType='Resource'>"
        b"<stEvt:action>saved</stEvt:action>"
        b"</rdf:li></rdf:Seq></xmpMM:History>"
        b"</rdf:Description>"
        b"</rdf:RDF></x:xmpmeta>"
    )


def write_metadata_webp(output, base_chunks, exif):
    vp8x = bytes((EXIF_FLAG | XMP_FLAG, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    chunks = write_chunk(b"VP8X", vp8x)
    for chunk_type, payload in base_chunks:
        if chunk_type not in (b"VP8X", b"EXIF", b"XMP "):
            chunks += write_chunk(chunk_type, payload)
    chunks += write_chunk(b"EXIF", exif)
    chunks += write_chunk(b"XMP ", make_xmp())

    body = b"WEBP" + chunks
    with open(output, "wb") as output_file:
        output_file.write(b"RIFF" + struct.pack("<I", len(body)) + body)


with open(BASE, "rb") as input_file:
    base_chunks = read_chunks(input_file.read())

tiff = make_tiff()
write_metadata_webp(TIFF_OUTPUT, base_chunks, tiff)
write_metadata_webp(PREFIXED_OUTPUT, base_chunks, b"Exif\x00\x00" + tiff)
