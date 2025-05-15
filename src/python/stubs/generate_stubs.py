"""
Script to generate pyi stubs by patching and calling mypy's stubgen tool.

There are two entry-points which are designed to call this script:
 - `make pystubs` is called during local development to generate the
   stubs and copy them into the git repo to be committed and reviewed.
 - in CI, the cibuildwheel action is used to validate that the stubs match what
   has been committed to the repo.

The depdendencies for the script are defined in pyproject.toml.
"""

from __future__ import absolute_import, annotations, division, print_function

import mypy.stubgen
import mypy.stubgenc
from mypy.stubgenc import SignatureGenerator, DocstringSignatureGenerator

from stubgenlib.siggen import (
    AdvancedSignatureGenerator,
    AdvancedSigMatcher,
)
from stubgenlib.utils import add_positional_only_args


PY_TO_STDVECTOR_ARG = "float | typing.Iterable[float]"


class OIIOSignatureGenerator(AdvancedSignatureGenerator):
    sig_matcher = AdvancedSigMatcher(
        signature_overrides={
            # signatures for these special methods include many inaccurate overloads
            "*.__ne__": "(self, other: object) -> bool",
            "*.__eq__": "(self, other: object) -> bool",
        },
        arg_type_overrides={
            # FIXME: Buffer may in fact be more accurate here
            ("*", "*", "Buffer"): "numpy.ndarray",
            # these use py_to_stdvector util
            ("*.ImageBufAlgo.*", "min", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "max", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "black", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "white", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "sthresh", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "scontrast", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "white_balance", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "values", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "top", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "bottom", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "topleft", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "topright", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "bottomleft", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "bottomright", "object"): PY_TO_STDVECTOR_ARG,
            ("*.ImageBufAlgo.*", "color", "object"): PY_TO_STDVECTOR_ARG,
            # BASETYPE & str are implicitly converible to TypeDesc
            ("*", "*", "*.TypeDesc"): "Union[TypeDesc, BASETYPE, str]",
            # list is not strictly required
            ("*.ImageOutput.open", "specs", "list[ImageSpec]"): "typing.Iterable[ImageSpec]",
        },
        result_type_overrides={
            # FIXME: is there a way to use std::optional for these?
            ("*.ImageOutput.create", "object"): "ImageOutput | None",
            ("*.ImageOutput.open", "object"): "ImageOutput | None",
            ("*.ImageInput.create", "object"): "ImageInput | None",
            ("*.ImageInput.open", "object"): "ImageInput | None",

            # ("*.TextureSystem.imagespec", "object"): "ImageSpec | None",

            # if you return an uninitialized unique_ptr to pybind11 it will convert to `None` (see #4685)
            ("*.ImageInput.read_native_deep_*", "DeepData"): "DeepData | None",

            # pybind11 has numpy support, so it may be possible to get it to emit these types
            # by using py::numpy in our wrapper code.
            ("*.ImageInput.read_*", "object"): "numpy.ndarray | None",
            ("*", "Buffer"): "numpy.ndarray",
            ("*.get_pixels", "object"): "numpy.ndarray | None",

            # For results, `object` is too restrictive (produces spurious errors during type analysis)
            ("*.getattribute", "object"): "typing.Any",
            ("*.ImageSpec.get", "object"): "typing.Any",

            ("*.ImageBufAlgo.histogram", "*"): "tuple[int, ...]",
            # pybind11 treats std:vector<T> as list[T], but we want tuple[T, ...]
            # our custom code to convert vector to tuple obscures the contained type.
            ("*.ImageBufAlgo.isConstantColor", "*"): "tuple[float, ...] | None",
            ("*.ImageBufAlgo.color_range_check", "*"): "tuple[int, ...] | None",

            ("*.TextureSystem.imagespec", "object"): "ImageSpec | None",
            ("*.TextureSystem.texture", "tuple"): "tuple[float, ...]",
            ("*.TextureSystem.texture3d", "tuple"): "tuple[float, ...]",
            ("*.TextureSystem.environment", "tuple"): "tuple[float, ...]",

            ("*.ImageBuf.getpixel", "tuple"): "tuple[float, ...]",
            ("*.ImageBuf.interppixel*", "tuple"): "tuple[float, ...]",
            ("*.ImageSpec.get_channelformats", "tuple"): "tuple[TypeDesc, ...]",
        },

        property_type_overrides={
            # FIXME: this isn't working
            ("*.ParamValue.value", "object"): "typing.Any",
        },
    )

    def process_sig(
        self, ctx: mypy.stubgen.FunctionContext, sig: mypy.stubgen.FunctionSig
    ) -> mypy.stubgen.FunctionSig:
        # Analyze the signature and add a '/' argument if necessary to mark
        # arguments which cannot be access by name.
        return add_positional_only_args(ctx, super().process_sig(ctx, sig))


class InspectionStubGenerator(mypy.stubgenc.InspectionStubGenerator):
    def get_sig_generators(self) -> list[SignatureGenerator]:
        return [
            OIIOSignatureGenerator(
                fallback_sig_gen=DocstringSignatureGenerator(),
            )
        ]


mypy.stubgen.InspectionStubGenerator = InspectionStubGenerator  # type: ignore[attr-defined,misc]
mypy.stubgenc.InspectionStubGenerator = InspectionStubGenerator  # type: ignore[misc]


def get_colored_diff(old_text: str, new_text: str):
    """
    Generates a colored diff between two strings.

    Returns:
        A string containing the colored diff output.
    """
    import difflib

    red = '\033[31m'
    green = '\033[32m'
    reset = '\033[0m'

    diff = difflib.unified_diff(
        old_text.splitlines(keepends=True),
        new_text.splitlines(keepends=True),
        lineterm="",
    )
    lines = []
    for line in diff:
        if line.startswith('-'):
            lines.append(f"{red}{line}{reset}")
        elif line.startswith('+'):
            lines.append(f"{green}{line}{reset}")
        else:
            lines.append(line)
    return "".join(lines)


def main() -> None:
    import argparse
    import pathlib
    import os
    import sys

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out-path",
        default="out",
        help="Directory to write the stubs."
    )
    parser.add_argument(
        "--validate-path",
        default=None,
        help="If provided, compare the generated stub to this file. Exits with code 2 if the "
             "contents differ."
    )
    args = parser.parse_args()
    out_path = pathlib.Path(args.out_path)
    print(f"Stub output directory: {out_path}")

    # perform import so we can see the traceback if it fails.
    import OpenImageIO

    sys.argv[1:] = ["-p", "OpenImageIO", "-o", str(out_path)]
    mypy.stubgen.main()
    source_path = out_path.joinpath("OpenImageIO", "OpenImageIO.pyi")
    if not source_path.exists():
        print("Stub generation failed")
        sys.exit(1)

    dest_path = out_path.joinpath("OpenImageIO", "__init__.pyi")
    print(f"Renaming to {dest_path}")
    os.rename(source_path, dest_path)

    new_text = dest_path.read_text()
    new_text = (
        "#\n# This file is auto-generated. DO NOT MODIFY!  "
        "Run `make pystubs` to regenerate\n#\n\n"
    ) + new_text
    dest_path.write_text(new_text)

    if args.validate_path and os.environ.get("GITHUB_ACTIONS", "false").lower() == "true":
        # in CI, validate that what has been committed to the repo is what we expect.
        validate_path = pathlib.Path(args.validate_path)

        print("Validating stubs against repository")
        print(f"Comparing {dest_path} to {validate_path}")

        old_text = validate_path.read_text()

        if old_text != new_text:
            print("Stub verification failed!")
            print("Changes to the source code have resulted in a change to the stubs.")
            print(get_colored_diff(old_text, new_text))
            print("Run `make pystubs` locally and commit the results for review.")
            print("The resulting __init__.pyi file will be uploaded as an artifact on this job.")
            sys.exit(2)


if __name__ == "__main__":
    main()
