"""Code shared by the pystencils-sfg generator scripts, kernels/<kernel>/<kernel>_pystencils.py.

Each script puts this directory on sys.path and imports the module. It holds the namespace of
the generated code, the --target option with its resolution to a pystencils SIMD target, and
the pystencils-sfg and kernel configurations, so that the scripts differ only in the kernel.
Changing this file regenerates every kernel (kernels/CMakeLists.txt lists it as a dependency).

The only script option is --target, the pystencils SIMD target (default CurrentCPU, the most
capable vector CPU target of the machine the generator runs on). The target must match what
PSB_ARCH_FLAGS enables, because the generated intrinsics are compiled with those flags.
"""

import argparse
import sys

import pystencils as ps
from pystencilssfg import SfgConfig

# Namespace of the generated code. It must differ from the backend tag
# backends::pystencils and from the repository's namespace kernels (pystencils-sfg puts
# the kernels themselves into <outer namespace>::kernels).
OUTER_NAMESPACE = "psb_generated"

# Targets worth offering. CurrentCPU is resolved below; the others are pystencils.Target members.
TARGETS = ("CurrentCPU", "X86_SSE", "X86_AVX", "X86_AVX512", "ARM_NEON")


def parse_args(prog, argv):
    parser = argparse.ArgumentParser(
        prog=prog,
        description="Generator script for a pystencils STREAM kernel.",
    )
    parser.add_argument(
        "--target",
        choices=TARGETS,
        default="CurrentCPU",
        help="pystencils SIMD target (default: %(default)s)",
    )
    return parser.parse_args(argv)


def resolve_target(prog, name):
    """Return a concrete vector CPU target; never silently fall back to scalar code."""
    if name == "CurrentCPU":
        target = ps.Target.auto_cpu()
    else:
        target = ps.Target[name]
    if not target.is_vector_cpu():
        sys.exit(
            f"{prog}: target '{name}' resolved to {target}, which is not a vector "
            "CPU target. For CurrentCPU pystencils needs py-cpuinfo; otherwise pass an explicit "
            "target with -DPSB_PYSTENCILS_TARGET=..."
        )
    return target


def make_sfg_config():
    sfg_config = SfgConfig()
    sfg_config.outer_namespace = OUTER_NAMESPACE
    return sfg_config


def make_kernel_config(target):
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
    return cfg
