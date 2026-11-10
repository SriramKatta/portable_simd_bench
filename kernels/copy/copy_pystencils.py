"""pystencils-sfg generator script for the STREAM copy kernel, dst[i] = src[i] over double.

Run by CMake (see cmake/pystencils.cmake and kernels/CMakeLists.txt), which compiles the
two generated files, copy_pystencils.hpp and copy_pystencils.cpp, into kernels_pystencils.
The generated header declares

    namespace psb_generated
    {
        void copy(std::span<double> dst, std::span<const double> src);
    }

and kernels/copy/pystencils.cpp, the hand-written wrapper, calls it. Keep the two in sync.

The only script option is --target, the pystencils SIMD target (default CurrentCPU, the most
capable vector CPU target of the machine the generator runs on). The target must match what
PSB_ARCH_FLAGS enables, because the generated intrinsics are compiled with those flags.

Contract implemented (see kernels/copy/copy.hpp): every element of [0, n) is copied, with no
alignment assumption on either pointer and any n, including 0.
"""

import argparse
import sys

import pystencils as ps
from pystencilssfg import SfgConfig, SourceFileGenerator
from pystencilssfg.lang.cpp import std

# Namespace of the generated code. It must differ from the backend tag
# backends::pystencils and from the repository's namespace kernels (pystencils-sfg puts
# the kernels themselves into <outer namespace>::kernels).
OUTER_NAMESPACE = "psb_generated"

# Targets worth offering. CurrentCPU is resolved below; the others are pystencils.Target members.
TARGETS = ("CurrentCPU", "X86_SSE", "X86_AVX", "X86_AVX512", "ARM_NEON")


def parse_args(argv):
    parser = argparse.ArgumentParser(
        prog="copy_pystencils.py",
        description="Generator script for the pystencils copy kernel.",
    )
    parser.add_argument(
        "--target",
        choices=TARGETS,
        default="CurrentCPU",
        help="pystencils SIMD target (default: %(default)s)",
    )
    return parser.parse_args(argv)


def resolve_target(name):
    """Return a concrete vector CPU target; never silently fall back to scalar code."""
    if name == "CurrentCPU":
        target = ps.Target.auto_cpu()
    else:
        target = ps.Target[name]
    if not target.is_vector_cpu():
        sys.exit(
            f"copy_pystencils.py: target '{name}' resolved to {target}, which is not a vector "
            "CPU target. For CurrentCPU pystencils needs py-cpuinfo; otherwise pass an explicit "
            "target with -DPSB_PYSTENCILS_TARGET=..."
        )
    return target


sfg_config = SfgConfig()
sfg_config.outer_namespace = OUTER_NAMESPACE

# keep_unknown_argv: the options that CMake passes through SCRIPT_ARGS end up in
# sfg.context.argv, with pystencils-sfg's own options already stripped.
with SourceFileGenerator(sfg_config, keep_unknown_argv=True) as sfg:
    args = parse_args(sfg.context.argv)
    target = resolve_target(args.target)
    print(f"copy_pystencils.py: pystencils target {target}", file=sys.stderr)

    src, dst = ps.fields("src, dst: double[1D]")
    assignment = ps.Assignment(dst(0), src(0))

    cfg = ps.CreateKernelConfig(
        target=target,
        # Iterate over the whole field: no ghost layers are skipped.
        ghost_layers=0,
    )
    cfg.cpu.openmp.enable = False
    cfg.cpu.vectorize.enable = True
    # std::span is contiguous, so the stride is 1. Without this assumption pystencils emits
    # strided (gather/scatter) accesses instead of packed loads and stores.
    cfg.cpu.vectorize.assume_inner_stride_one = True
    # The contract has no alignment requirement and nothing speculative is wanted.
    cfg.cpu.vectorize.assume_aligned = False
    cfg.cpu.vectorize.use_nontemporal_stores = False

    # The vectorizer emits a SIMD loop over the largest multiple of the vector width and
    # a scalar loop for the remainder.
    copy_kernel = sfg.kernels.create(assignment, "copy_kernel", cfg)

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
