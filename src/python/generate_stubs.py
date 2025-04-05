from __future__ import absolute_import, annotations, division, print_function

import mypy.stubgen
import mypy.stubgenc
from mypy.stubgenc import SignatureGenerator, DocstringSignatureGenerator

from stubgenlib.siggen import (
    AdvancedSignatureGenerator,
    AdvancedSigMatcher,
)
from stubgenlib.utils import add_positional_only_args


PY_TO_STDVECTOR_RESULT = "float | list[float] | tuple[float, ...]"


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
            ("*.ImageBufAlgo.*", "min", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "max", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "black", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "white", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "sthresh", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "scontrast", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "white_balance", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "values", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "top", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "bottom", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "topleft", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "topright", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "bottomleft", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "bottomright", "object"): PY_TO_STDVECTOR_RESULT,
            ("*.ImageBufAlgo.*", "color", "object"): PY_TO_STDVECTOR_RESULT,
            # BASETYPE & str are implicitly converible to TypeDesc
            ("*", "*", "*.TypeDesc"): "Union[TypeDesc, BASETYPE, str]",
            # list is not strictly required
            ("*.ImageOutput.open", "specs", "list[ImageSpec]"): "Iterable[ImageSpec]",
        },
        result_type_overrides={
            # FIXME: is there a way to use std::optional for these?
            ("*.ImageOutput.create", "object"): "ImageOutput | None",
            ("*.ImageOutput.open", "object"): "ImageOutput | None",
            ("*.ImageInput.create", "object"): "ImageInput | None",
            ("*.ImageInput.open", "object"): "ImageInput | None",

            # ("*.TextureSystem.imagespec", "object"): "ImageSpec | None",
            # ("*.ImageInput.read_native_deep_*", "object"): "DeepData | None",

            # pybind11 has special support, so it may be possible to get it to emit these types
            # by using py::numpy in our wrapper code.
            ("*.ImageInput.read_*", "object"): "numpy.ndarray | None",
            ("*", "Buffer"): "numpy.ndarray",
            ("*.get_pixels", "object"): "numpy.ndarray | None",

            # For results, object is too restrictive (produces spurious errors during type analysis)
            ("*.getattribute", "object"): "Any",
            ("*.ImageSpec.get", "object"): "Any",

            # pybind11 does not have a way to emit tuple[T, ...], e.g. from std:vector<T>.
            ("*.ImageBufAlgo.histogram", "tuple"): "tuple[int, ...]",
            ("*.ImageBufAlgo.isConstantColor", "*"): "tuple[float, ...] | None",
            ("*.ImageBufAlgo.color_range_check", "*"): "tuple[int, ...] | None",
            ("*.TextureSystem.texture", "tuple"): "tuple[float, ...]",
            ("*.TextureSystem.texture3d", "tuple"): "tuple[float, ...]",
            ("*.TextureSystem.environment", "tuple"): "tuple[float, ...]",
            ("*.ImageBuf.getpixel", "tuple"): "tuple[float, ...]",
            ("*.ImageBuf.interpixel*", "tuple"): "tuple[float, ...]",
            ("*.ImageSpec.get_channelformats", "tuple"): "tuple[TypeDesc, ...]",
        },

        property_type_overrides={
            # FIXME: this isn't working
            ("*.ParamValue.value", "object"): "Any",
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

    def set_defined_names(self, defined_names: set[str]) -> None:
        super().set_defined_names(defined_names)
        for typ in ["Any", "Iterable"]:
            self.add_name(f"typing.{typ}", require=False)


mypy.stubgen.InspectionStubGenerator = InspectionStubGenerator  # type: ignore[attr-defined,misc]
mypy.stubgenc.InspectionStubGenerator = InspectionStubGenerator  # type: ignore[misc]

if __name__ == "__main__":
    mypy.stubgen.main()
