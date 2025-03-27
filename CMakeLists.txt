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
        DOWNLOAD https://raw.githubusercontent.com/pure-data/pd.cmake/refs/tags/v0.1.0/pd.cmake ${PDCMAKE_FILE}
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS)
endif()
include(${PDCMAKE_FILE})

# ╭──────────────────────────────────────╮
# │               OpenBLAS               │
# ╰──────────────────────────────────────╯
if(NOT CMAKE_SYSTEM_NAME STREQUAL "APPLE")
    set(BUILD_STATIC_LIBS ON)
    set(BUILD_WITHOUT_LAPACK OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_LAPACK_DEPRECATED OFF)
    set(C_LAPACK ON)
    set(CBLAS OFF)

    # Build OpenBLAS
    add_subdirectory(Libraries/OpenBLAS EXCLUDE_FROM_ALL)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/OpenBLAS/lapack-netlib/CBLAS/include/")

    # can't figure how to generate this
    set(CBLAS_MANGLING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/cblas_mangling.h")
    file(WRITE "${CBLAS_MANGLING_HEADER}" "")
    include_directories("${CMAKE_CURRENT_BINARY_DIR}/")

    # Build LAPACKE
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/OpenBLAS/lapack-netlib/LAPACKE/include/")
    set(LAPACKE_LIBRARY openblas)
    set(OPENBLAS_LIBRARY openblas)
endif()

# ╭──────────────────────────────────────╮
# │                 SAF                  │
# ╰──────────────────────────────────────╯
set(SAF_BUILD_TESTS OFF)
set(SAF_BUILD_EXAMPLES OFF)
set(SAF_ENABLE_SOFA_READER_MODULE ON)
set(SAF_ENABLE_TRACKER_MODULE ON)
set(SAF_ENABLE_HADES_MODULE ON)

if(NOT APPLE)
    set(SAF_PERFORMANCE_LIB SAF_USE_OPEN_BLAS_AND_LAPACKE)
    add_definitions(-DSAF_USE_OPEN_BLAS_AND_LAPACKE)
else()
    set(SAF_PERFORMANCE_LIB "SAF_USE_APPLE_ACCELERATE")
endif()

add_subdirectory(Libraries/Spatial_Audio_Framework EXCLUDE_FROM_ALL)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/framework/include/")

# ╭──────────────────────────────────────╮
# │              PD OBJECTS              │
# ╰──────────────────────────────────────╯
file(GLOB ENCODER_SRC
     "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_enc/*.c")

pd_add_external(saf.encoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/encoder~.c;${ENCODER_SRC}" LINK_LIBRARIES
                saf)

# ──────────────────────────────────────
file(GLOB saf_dec_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_dec/*.c")
pd_add_external(saf.decoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/decoder~.c;${saf_dec_SRC}" LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(GLOB BINAURAL_TILDE_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_bin/*.c")
pd_add_external(saf.binaural~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binaural~.c;${BINAURAL_TILDE_SOURCE}" LINK_LIBRARIES
                saf)

# ─────────────────────────────────────
file(GLOB ROOMSIM_TILDE_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_roomsim/*.c")

pd_add_external(saf.roomsim~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/roomsim~.c;${ROOMSIM_TILDE_SOURCE}" LINK_LIBRARIES
                saf)

# ─────────────────────────────────────
file(GLOB PITCHSHIFTER_TILDE_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/pitch_shifter/*.c")

pd_add_external(saf.pitchshifter~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/pitchshifter~.c;${PITCHSHIFTER_TILDE_SOURCE}" LINK_LIBRARIES
                saf)

# ╭──────────────────────────────────────╮
# │              DATA FILES              │
# ╰──────────────────────────────────────╯
file(GLOB HELP_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.pd")
file(GLOB SOUNDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.wav")
pd_add_datafile(saf.binaural~ "${HELP_PATCHES}")
pd_add_datafile(saf.binaural~ "${SOUNDS}")
