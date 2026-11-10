"""pystencils-sfg generator script for the STREAM scale kernel, dst[i] = scalar * src[i] over double.

Run by CMake (see cmake/pystencils.cmake and kernels/CMakeLists.txt), which compiles the
two generated files, scale_pystencils.hpp and scale_pystencils.cpp, into kernels_pystencils.
The generated header declares

    namespace psb_generated
    {
        void scale(std::span<double> dst, std::span<const double> src, double scalar);
    }

and kernels/scale/pystencils.cpp, the hand-written wrapper, calls it. Keep the two in sync.

The scalar is a typed pystencils symbol of type double and a by-value parameter of the generated function.

The only script option is --target, see kernels/pystencils_common.py, which also holds the
configuration shared by all kernels.

Contract implemented (see kernels/scale/scale.hpp): dst[i] = scalar * src[i] for every element of [0, n), with no
alignment assumption on any pointer and any n, including 0.
"""

import sys
from pathlib import Path

# The shared module lives in the parent directory (kernels/).
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pystencils as ps
import pystencils_common as common
from pystencilssfg import SourceFileGenerator
from pystencilssfg.lang.cpp import std

PROG = Path(__file__).name

# keep_unknown_argv: the options that CMake passes through SCRIPT_ARGS end up in
# sfg.context.argv, with pystencils-sfg's own options already stripped.
with SourceFileGenerator(common.make_sfg_config(), keep_unknown_argv=True) as sfg:
    args = common.parse_args(PROG, sfg.context.argv)
    target = common.resolve_target(PROG, args.target)
    print(f"{PROG}: pystencils target {target}", file=sys.stderr)

    src, dst = ps.fields("src, dst: double[1D]")
    scalar_symbol = ps.TypedSymbol("scalar", "double")
    assignment = ps.Assignment(dst(0), scalar_symbol * src(0))

    # The vectorizer emits a SIMD loop over the largest multiple of the vector width and
    # a scalar loop for the remainder.
    scale_kernel = sfg.kernels.create(assignment, "scale_kernel", common.make_kernel_config(target))

    # The interface is std::span rather than the raw kernel parameters (data pointers, sizes,
    # strides), because the latter are named and ordered by pystencils and not documented.
    # The sources are read-only, so their spans have a const element type.
    dst_span = std.span.from_field(dst)
    src_span = std.span("const double").var("src")
    scalar = sfg.var("scalar", "double")

    sfg.function("scale").params(dst_span, src_span, scalar)(
        sfg.map_field(dst, dst_span),
        sfg.map_field(src, src_span),
        sfg.call(scale_kernel),
    )
