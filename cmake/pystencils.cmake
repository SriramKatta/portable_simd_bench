# Optional pystencils backend: detection, and psb_target_pystencils_sources() to generate
# code from pystencils-sfg scripts and compile it into a target. Detection sets
# PSB_HAVE_PYSTENCILS; kernels/CMakeLists.txt calls the function only when it is set.
#
# Neither pystencils nor pystencils-sfg is ever installed from CMake. To point CMake at
# a Python environment that has both:
#   cmake -S . -B build -DPystencilsSfg_PYTHON_INTERPRETER=/path/to/venv/bin/python
# This is the cache variable of pystencils-sfg's own CMake support. If it is not set,
# the hint PystencilsSfg_PYTHON_PATH is used, and failing that FindPython, which takes
# its usual hints (Python_EXECUTABLE, Python_ROOT_DIR, ...), in that order.

option(PSB_ENABLE_PYSTENCILS "Build the pystencils backend if pystencils and pystencils-sfg are available" ON)

# The pystencils SIMD target (a pystencils.Target member name), passed to the
# generator scripts. CurrentCPU is the most capable vector target of the CPU the
# generator runs on, which is also the CPU -march=native in PSB_ARCH_FLAGS refers to.
# It must match what PSB_ARCH_FLAGS enables: the generated intrinsics are compiled with
# those flags. For example with -DPSB_ARCH_FLAGS=-march=haswell use X86_AVX, not
# X86_AVX512.
set(PSB_PYSTENCILS_TARGET "CurrentCPU" CACHE STRING
    "pystencils SIMD target for the pystencils backend; must match PSB_ARCH_FLAGS")
set_property(CACHE PSB_PYSTENCILS_TARGET PROPERTY STRINGS
    CurrentCPU X86_SSE X86_AVX X86_AVX512 ARM_NEON)

set(PSB_HAVE_PYSTENCILS OFF)
if(PSB_ENABLE_PYSTENCILS)
    # Pick the interpreter with the same precedence FindPystencilsSfg.cmake uses.
    set(psb_python "")
    if(DEFINED CACHE{PystencilsSfg_PYTHON_INTERPRETER})
        set(psb_python "${PystencilsSfg_PYTHON_INTERPRETER}")
    elseif(DEFINED PystencilsSfg_PYTHON_PATH)
        set(psb_python "${PystencilsSfg_PYTHON_PATH}")
    else()
        find_package(Python COMPONENTS Interpreter QUIET)
        if(Python_Interpreter_FOUND)
            set(psb_python "${Python_EXECUTABLE}")
        endif()
    endif()

    set(psb_python_ok FALSE)
    if(psb_python)
        execute_process(
            COMMAND "${psb_python}" -c "import pystencils, pystencilssfg"
            RESULT_VARIABLE psb_import_result
            OUTPUT_QUIET ERROR_QUIET)
        if(psb_import_result EQUAL 0)
            set(psb_python_ok TRUE)
        endif()
    endif()

    if(psb_python_ok)
        # pystencils-sfg's documented way: create the Find-module with
        # `sfg-cli cmake make-find-module` (the same entry point as
        # `python -m pystencilssfg cmake make-find-module`) in a directory on the
        # CMake module path, then find_package(PystencilsSfg). It is created in the
        # build tree so that it always belongs to the installed version.
        set(psb_find_dir "${CMAKE_BINARY_DIR}/pystencilssfg_cmake")
        file(MAKE_DIRECTORY "${psb_find_dir}")
        execute_process(
            COMMAND "${psb_python}" -m pystencilssfg cmake make-find-module
            WORKING_DIRECTORY "${psb_find_dir}"
            RESULT_VARIABLE psb_find_module_result
            OUTPUT_QUIET ERROR_QUIET)
        if(psb_find_module_result EQUAL 0)
            list(APPEND CMAKE_MODULE_PATH "${psb_find_dir}")
            set(PystencilsSfg_PYTHON_PATH "${psb_python}")
            find_package(PystencilsSfg QUIET)
            if(PystencilsSfg_FOUND)
                set(PSB_HAVE_PYSTENCILS ON)
            endif()
        endif()
    endif()

    if(NOT PSB_HAVE_PYSTENCILS)
        if(NOT psb_python)
            set(psb_python "<none found>")
        endif()
        message(STATUS "pystencils and pystencils-sfg are not usable (Python: ${psb_python}): "
                       "the pystencils backend is skipped (point CMake at a Python that has both "
                       "packages with -DPystencilsSfg_PYTHON_INTERPRETER=/path/to/venv/bin/python)")
    endif()
endif()

# Generate C++ from pystencils-sfg scripts and compile it as part of an existing target.
#
#   psb_target_pystencils_sources(<target> SCRIPTS <x.py>...
#                                 [PYSTENCILS_TARGET <t>] [SCRIPT_ARGS <arg>...]
#                                 [DEPENDS <file>...])
#
# SCRIPTS            generator scripts; relative paths resolve against the calling
#                    directory's CMAKE_CURRENT_SOURCE_DIR.
# PYSTENCILS_TARGET  pystencils SIMD target passed to the scripts as --target=<t>
#                    (default ${PSB_PYSTENCILS_TARGET}).
# SCRIPT_ARGS        further arguments for the scripts, after --target.
# DEPENDS            further files the generation depends on, e.g. a Python module the scripts
#                    import; relative paths resolve like SCRIPTS. Editing one regenerates the code.
#
# Wraps pystencilssfg_generate_target_sources(): the generated <script stem>.cpp is added
# to the sources of <target> and so is compiled with <target>'s own flags, and the
# generated headers are included as "gen/<script stem>.hpp" (the directory is put on the
# include path of <target>), so script stems must be unique within a target. Changing
# the pystencils target or SCRIPT_ARGS regenerates the code. It is an error when
# pystencils-sfg was not found (PSB_HAVE_PYSTENCILS is false), so guard the call with
# if(PSB_HAVE_PYSTENCILS).
function(psb_target_pystencils_sources target)
    cmake_parse_arguments(ARG "" "PYSTENCILS_TARGET" "SCRIPTS;SCRIPT_ARGS;DEPENDS" ${ARGN})
    if(NOT PSB_HAVE_PYSTENCILS)
        message(FATAL_ERROR "psb_target_pystencils_sources: pystencils and pystencils-sfg "
                            "were not found (PSB_HAVE_PYSTENCILS is false)")
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "psb_target_pystencils_sources: '${target}' is not a target")
    endif()
    if(NOT ARG_SCRIPTS)
        message(FATAL_ERROR "psb_target_pystencils_sources(${target}): SCRIPTS is empty")
    endif()
    if(NOT ARG_PYSTENCILS_TARGET)
        set(ARG_PYSTENCILS_TARGET ${PSB_PYSTENCILS_TARGET})
    endif()

    set(scripts "")
    foreach(script IN LISTS ARG_SCRIPTS)
        cmake_path(ABSOLUTE_PATH script BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   NORMALIZE OUTPUT_VARIABLE abs_script)
        list(APPEND scripts "${abs_script}")
    endforeach()

    set(depends "")
    foreach(file IN LISTS ARG_DEPENDS)
        cmake_path(ABSOLUTE_PATH file BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   NORMALIZE OUTPUT_VARIABLE abs_file)
        list(APPEND depends "${abs_file}")
    endforeach()

    # The target and the extra arguments reach the scripts only as command-line
    # arguments, which do not by themselves trigger regeneration. This file is
    # rewritten only when its content changes, and the generation depends on it.
    # file(CONFIGURE @ONLY) reads variables of this function scope, hence the copies.
    set(stamp_target "${ARG_PYSTENCILS_TARGET}")
    list(JOIN ARG_SCRIPT_ARGS " " stamp_args)
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/${target}_pystencils_target.txt)
    file(CONFIGURE OUTPUT ${stamp} CONTENT "@stamp_target@\n@stamp_args@\n" @ONLY)

    pystencilssfg_generate_target_sources(${target}
        SCRIPTS ${scripts}
        SCRIPT_ARGS --target=${ARG_PYSTENCILS_TARGET} ${ARG_SCRIPT_ARGS}
        DEPENDS ${stamp} ${depends})
endfunction()
