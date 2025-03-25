cmake_minimum_required(VERSION 3.25)
project(saf)

# ╭──────────────────────────────────────╮
# │               pd.cmake               │
# ╰──────────────────────────────────────╯
cmake_policy(SET CMP0135 NEW)
set(PDCMAKE_FILE ${CMAKE_BINARY_DIR}/pd.cmake)
if(NOT EXISTS ${PDCMAKE_FILE})
  message(STATUS "Downloading pd.cmake")
  file(
    DOWNLOAD
    https://raw.githubusercontent.com/pure-data/pd.cmake/refs/tags/v0.1.0/pd.cmake
    ${PDCMAKE_FILE}
    SHOW_PROGRESS
    STATUS DOWNLOAD_STATUS)
endif()
include(${PDCMAKE_FILE})

# ╭──────────────────────────────────────╮
# │               OpenBLAS               │
# ╰──────────────────────────────────────╯
set(BUILD_STATIC_LIBS ON)
set(BUILD_WITHOUT_LAPACK OFF)
add_subdirectory(Libraries/OpenBLAS EXCLUDE_FROM_ALL)

# Build LAPACKE as a static library from the object library LAPACKE
add_library(lapacke_static STATIC $<TARGET_OBJECTS:LAPACKE>)
# Link the OpenBLAS target so that LAPACKE has the required routines
target_link_libraries(lapacke_static PUBLIC openblas)

set(LAPACKE_LIBRARY lapacke_static)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/OpenBLAS/lapack-netlib/LAPACKE/include/"
)
# ╭──────────────────────────────────────╮
# │                 SAF                  │
# ╰──────────────────────────────────────╯
set(SAF_BUILD_TESTS OFF)
set(SAF_BUILD_EXAMPLES OFF)
set(SAF_PERFORMANCE_LIB SAF_USE_OPEN_BLAS_AND_LAPACKE)
add_subdirectory(Libraries/Spatial_Audio_Framework EXCLUDE_FROM_ALL)
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/framework/include/"
)
add_definitions(-DSAF_USE_OPEN_BLAS_AND_LAPACKE)

# ╭──────────────────────────────────────╮
# │              PD OBJECTS              │
# ╰──────────────────────────────────────╯
file(GLOB ENCODER_TILDE_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/Sources/encoder~/*.c")

pd_add_external(saf.encoder~ "${ENCODER_TILDE_SOURCE}" LINK_LIBRARIES openblas
                lapacke_static saf)

# ─────────────────────────────────────
file(GLOB DECODER_TILDE_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/Sources/decoder~/*.c")

pd_add_external(saf.decoder~ "${DECODER_TILDE_SOURCE}" LINK_LIBRARIES openblas
                lapacke_static saf)


# ─────────────────────────────────────
# file( GLOB BINAURAL_TILDE_SOURCE
# "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_bin/*.c"
# ) pd_add_external( saf.binaural~
# "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binaural~.c;${BINAURAL_TILDE_SOURCE}"
# LINK_LIBRARIES openblas lapacke_static saf)
