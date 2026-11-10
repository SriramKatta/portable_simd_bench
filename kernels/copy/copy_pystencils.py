"""pystencils-sfg generator script for the STREAM copy kernel, dst[i] = src[i] over double.

Run by CMake (see cmake/pystencils.cmake and kernels/CMakeLists.txt), which compiles the
two generated files, copy_pystencils.hpp and copy_pystencils.cpp, into kernels_pystencils.
The generated header declares

    namespace psb_generated
    {
        void copy(std::span<double> dst, std::span<const double> src);
    }

and kernels/copy/pystencils.cpp, the hand-written wrapper, calls it. Keep the two in sync.

The only script option is --target, see kernels/pystencils_common.py, which also holds the
configuration shared by all kernels.

Contract implemented (see kernels/copy/copy.hpp): every element of [0, n) is copied, with no
alignment assumption on either pointer and any n, including 0.
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
    assignment = ps.Assignment(dst(0), src(0))

    # The vectorizer emits a SIMD loop over the largest multiple of the vector width and
    # a scalar loop for the remainder.
    copy_kernel = sfg.kernels.create(assignment, "copy_kernel", common.make_kernel_config(target))

    # The interface is std::span rather than the raw kernel parameters (data pointers, sizes,
    # strides), because the latter are named and ordered by pystencils and not documented.
    # src is read-only, so its span has a const element type.
    dst_span = std.span.from_field(dst)
    src_span = std.span("const double").var("src")

    sfg.function("copy").params(dst_span, src_span)(
        sfg.map_field(dst, dst_span),
        sfg.map_field(src, src_span),
        sfg.call(copy_kernel),
    )
