# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0

import numpy as np
import struct

import OpenImageIO as oiio


def check(ok, message):
    if not ok:
        raise AssertionError(message)


def create_spec(encoder, channels, fps=None):
    spec = oiio.ImageSpec(8, 8, channels, oiio.UINT8)
    # Use lossless so result remains consistent
    spec.attribute("compression", "lossless")
    spec.attribute("webp:use_animation_encoder", encoder)
    # Don't premultiply and un-premultiply by alpha
    spec.attribute("oiio:UnassociatedAlpha", 1)
    spec.attribute("oiio:LoopCount", 3)
    if fps is not None:
        spec.attribute("FramesPerSecond", float(fps))
    return spec


def read_chunks(filename):
    with open(filename, "rb") as input_file:
        data = input_file.read()

    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WEBP":
        raise RuntimeError(f"{filename} is not a RIFF WebP file")

    chunks = []
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise RuntimeError("truncated WebP chunk header")

        # 4-byte tag | 4-byte payload size | payload | optional 1-byte padding
        fourcc = data[offset : offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        begin = offset + 8
        end = begin + size
        if end > len(data):
            raise RuntimeError("truncated WebP chunk payload")
        chunks.append((fourcc, data[begin:end]))
        offset = end + (size & 1)
    return chunks    


def test_roundtrip(encoder, channels, frame_count):
    filename = f"frames-{encoder}-{channels}-{frame_count}.webp"
    expected = []

    # --- Write frames to image ---
    output = oiio.ImageOutput.create(filename)
    check(output is not None, oiio.geterror())
    for frame in range(frame_count):
        # Frame 1 inherits the first frame's FPS; frame 2 overrides it.
        fps = (10.0, None, 25.0)[frame]
        spec = create_spec(encoder, channels, fps)
        mode = "Create" if frame == 0 else "AppendSubimage"
        check(output.open(filename, spec, mode), output.geterror())

        pixels = np.zeros((8, 8, channels), dtype=np.uint8)
        pixels[..., frame] = 255
        if channels == 4:
            pixels[..., 3] = 128

        check(output.write_image(pixels), output.geterror())
        expected.append(pixels)

    check(output.close(), output.geterror())
    check(output.close(), "Repeated close failed")


    # --- Read frames from image ---
    config = oiio.ImageSpec()
    config.attribute("oiio:UnassociatedAlpha", 1)
    image = oiio.ImageInput.open(filename, config)
    check(image is not None, oiio.geterror())
    try:
        for frame, pixels in enumerate(expected):
            check(image.seek_subimage(frame, 0), image.geterror())
            actual = image.read_image(oiio.UINT8)
            check(actual is not None, image.geterror())
            # Frames were encoded losslessly, so they should match exactly
            np.testing.assert_array_equal(actual.reshape(pixels.shape), pixels)

        check(not image.seek_subimage(frame_count, 0), "Unexpected extra frame")
    finally:
        image.close()

    stored_data = read_chunks(filename)
    if frame_count > 1:
        # WebPInput does not expose the frame durations, so we have to parse the RIFF ourselves
        durations = [
            int.from_bytes(payload[12:15], "little")
            for tag, payload in stored_data if tag == b"ANMF"
        ]
        check(durations == [100, 100, 40], f"Wrong durations: {durations}")

        animation = [payload for tag, payload in stored_data if tag == b"ANIM"]
        check(len(animation) == 1, "Expected exactly one animation header")
        check(int.from_bytes(animation[0][4:6], "little") == 3,
              "Wrong loop count")
    else:
        check(not any(tag == b"ANMF" for tag, _ in stored_data),
              "Single image unexpectedly stored as animation")


def test_invalid_append(encoder):
    filename = f"invalid-append-{encoder}.webp"
    output = oiio.ImageOutput.create(filename)
    check(output is not None, oiio.geterror())
    check(output.open(filename, create_spec(encoder, 3)), output.geterror())
    check(output.write_image(np.zeros((8, 8, 3), dtype=np.uint8)),
          output.geterror())

    for width, channels in ((9, 3), (8, 4)):
        bad = create_spec(encoder, channels)
        bad.width = width
        check(not output.open(filename, bad, "AppendSubimage"),
              "Accepted incompatible frame")
        check(bool(output.geterror()), "Missing rejection diagnostic")

    check(output.close(), output.geterror())
    image = oiio.ImageInput.open(filename)
    check(image is not None, oiio.geterror())
    try:
        pixels = image.read_image(oiio.UINT8)
        check(pixels is not None, image.geterror())
        check(np.all(pixels == 0), "Pending frame corrupted")
        check(not image.seek_subimage(1, 0), "Rejected frame was written")
    finally:
        image.close()


for encoder in (0, 1): # test both encoder
    for channels in (3, 4): # test with and without transparency
        test_roundtrip(encoder, channels, 3) # 3 frames -> animated image
        test_roundtrip(encoder, channels, 1) # 1 frame -> still image

    test_invalid_append(encoder)
