# Export the compiler-emitted assembly (-S) of every registered target.
#
#   psb_export_asm(<target>)
#
# For each source of <target> this writes
#   ${PSB_ASM_DIR}/<source path relative to the target's source dir, .s>
# e.g. build/asm/copy/xsimd.s, ready for OSACA / llvm-mca or for diffing backends.
#
# Central switches:
#   target asm         builds every export: cmake --build build --target asm
#   PSB_EXPORT_ASM     (OFF) when ON, `asm` is part of the default build
#   PSB_ASM_DIR        output directory (default <build>/asm)
#   PSB_ASM_FLAGS      extra flags for the export ONLY (e.g. "-fverbose-asm -masm=intel")
#
# Why a mirror instead of a hand-written command line: the exported assembly
# must be what the benchmark runs. For each source we create an EXCLUDE_FROM_ALL
# OBJECT library that reads the original target's compile options, definitions,
# include directories, C++ standard settings and linked libraries (for their
# usage requirements, e.g. xsimd/hwy -isystem paths), so CMake itself generates
# the compile command. It differs only by -S (GCC/Clang then write assembly text
# to the "object" file, even though CMake also passes -c). A custom command copies
# that file to its final .s path. One helper per source keeps the mapping 1:1.
#
# Notes: compile options/definitions/includes are mirrored lazily (generator
# expressions); linked libraries and the property list below are read when
# psb_export_asm() is called, so call it after the target is fully set up.
# Per-source COMPILE_OPTIONS/COMPILE_DEFINITIONS are not mirrored.

option(PSB_EXPORT_ASM "Build the assembly export as part of the default build" OFF)
set(PSB_ASM_DIR "${CMAKE_BINARY_DIR}/asm" CACHE PATH "Directory receiving the exported assembly")
set(PSB_ASM_FLAGS "" CACHE STRING "Extra flags for the assembly export only (space-separated)")
separate_arguments(PSB_ASM_FLAGS_LIST UNIX_COMMAND "${PSB_ASM_FLAGS}")

set(psb_asm_all "")
if(PSB_EXPORT_ASM)
    set(psb_asm_all ALL)
endif()
add_custom_target(asm ${psb_asm_all} COMMENT "Exporting assembly")

# Target properties copied verbatim when set on the original target.
set(PSB_ASM_MIRRORED_PROPERTIES
    CXX_STANDARD CXX_STANDARD_REQUIRED CXX_EXTENSIONS
    C_STANDARD C_STANDARD_REQUIRED C_EXTENSIONS
    POSITION_INDEPENDENT_CODE CXX_VISIBILITY_PRESET C_VISIBILITY_PRESET
    VISIBILITY_INLINES_HIDDEN COMPILE_FLAGS COMPILE_FEATURES)

function(psb_export_asm target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "psb_export_asm: '${target}' is not a target")
    endif()
    get_target_property(src_dir ${target} SOURCE_DIR)
    get_target_property(sources ${target} SOURCES)
    get_target_property(libs ${target} LINK_LIBRARIES)

    set(outputs "")
    foreach(src IN LISTS sources)
        if(NOT src MATCHES "\\.(c|cc|cpp|cxx|c\\+\\+|C)$")
            continue()
        endif()
        cmake_path(ABSOLUTE_PATH src BASE_DIRECTORY "${src_dir}" NORMALIZE OUTPUT_VARIABLE abs_src)
        cmake_path(RELATIVE_PATH abs_src BASE_DIRECTORY "${src_dir}" OUTPUT_VARIABLE rel)
        if(rel MATCHES "^\\.\\.")
            message(FATAL_ERROR "psb_export_asm(${target}): source '${abs_src}' is outside "
                                "the target's source dir '${src_dir}'")
        endif()
        cmake_path(REPLACE_EXTENSION rel LAST_ONLY ".s" OUTPUT_VARIABLE rel_asm)
        set(out "${PSB_ASM_DIR}/${rel_asm}")

        get_property(taken GLOBAL PROPERTY PSB_ASM_OUTPUTS)
        if(out IN_LIST taken)
            message(FATAL_ERROR "psb_export_asm(${target}): '${abs_src}' maps to '${out}', "
                                "which is already produced by another registered source")
        endif()
        set_property(GLOBAL APPEND PROPERTY PSB_ASM_OUTPUTS "${out}")

        string(MAKE_C_IDENTIFIER "${target}_${rel}" id)
        set(helper psb_asm_${id})
        add_library(${helper} OBJECT EXCLUDE_FROM_ALL "${abs_src}")
        set_target_properties(${helper} PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
        target_compile_options(${helper} PRIVATE "$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>")
        target_compile_definitions(${helper} PRIVATE "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
        target_include_directories(${helper} PRIVATE "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>")
        if(libs)
            target_link_libraries(${helper} PRIVATE ${libs})
        endif()
        foreach(prop IN LISTS PSB_ASM_MIRRORED_PROPERTIES)
            get_target_property(value ${target} ${prop})
            if(NOT value STREQUAL "value-NOTFOUND")
                set_property(TARGET ${helper} PROPERTY ${prop} "${value}")
            endif()
        endforeach()
        # Export-only flags last so they can override mirrored ones.
        target_compile_options(${helper} PRIVATE -S ${PSB_ASM_FLAGS_LIST})

        cmake_path(GET out PARENT_PATH out_dir)
        add_custom_command(OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_OBJECTS:${helper}>" "${out}"
            DEPENDS ${helper} "$<TARGET_OBJECTS:${helper}>"
            COMMENT "Exporting assembly ${rel_asm}"
            VERBATIM COMMAND_EXPAND_LISTS)
        list(APPEND outputs "${out}")
    endforeach()

    if(outputs)
        add_custom_target(asm_${target} DEPENDS ${outputs})
        add_dependencies(asm asm_${target})
    endif()
endfunction()
