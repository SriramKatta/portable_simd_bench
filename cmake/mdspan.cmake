CPMAddPackage(
  NAME mdspan
  GITHUB_REPOSITORY kokkos/mdspan
  GIT_TAG single-header
)

add_library(mdspan_headers INTERFACE)

target_include_directories(mdspan_headers SYSTEM INTERFACE ${mdspan_SOURCE_DIR})
