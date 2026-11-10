#include <benchmark/benchmark.h>

// PSB_COMPILER_ID, PSB_COMPILER_VERSION and PSB_ARCH_FLAGS come from bench/CMakeLists.txt.

int main(int argc, char **argv)
{
    // Recorded so that saved result files say which compiler and flags produced them.
    benchmark::AddCustomContext("compiler", PSB_COMPILER_ID " " PSB_COMPILER_VERSION);
    benchmark::AddCustomContext("arch_flags", PSB_ARCH_FLAGS);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
    {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
