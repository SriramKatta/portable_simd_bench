"""pystencils-sfg generator script for the STREAM add kernel, dst[i] = a[i] + b[i] over double.

Run by CMake (see cmake/pystencils.cmake and kernels/CMakeLists.txt), which compiles the
two generated files, add_pystencils.hpp and add_pystencils.cpp, into kernels_pystencils.
The generated header declares

    namespace psb_generated
    {
        void add(std::span<double> dst, std::span<const double> a, std::span<const double> b);
    }

and kernels/add/pystencils.cpp, the hand-written wrapper, calls it. Keep the two in sync.

The only script option is --target, see kernels/pystencils_common.py, which also holds the
configuration shared by all kernels.

Contract implemented (see kernels/add/add.hpp): dst[i] = a[i] + b[i] for every element of [0, n), with no
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

    a, b, dst = ps.fields("a, b, dst: double[1D]")
    assignment = ps.Assignment(dst(0), a(0) + b(0))

    # The vectorizer emits a SIMD loop over the largest multiple of the vector width and
    # a scalar loop for the remainder.
    add_kernel = sfg.kernels.create(assignment, "add_kernel", common.make_kernel_config(target))

    # The interface is std::span rather than the raw kernel parameters (data pointers, sizes,
    # strides), because the latter are named and ordered by pystencils and not documented.
    # The sources are read-only, so their spans have a const element type.
    dst_span = std.span.from_field(dst)
    a_span = std.span("const double").var("a")
    b_span = std.span("const double").var("b")

    sfg.function("add").params(dst_span, a_span, b_span)(
        sfg.map_field(dst, dst_span),
        sfg.map_field(a, a_span),
        sfg.map_field(b, b_span),
        sfg.call(add_kernel),
    )
