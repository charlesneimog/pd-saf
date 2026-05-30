cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(saf)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_STATIC_LIBS ON)
set(BUILD_TESTING OFF)

# pd.cmake
set(PDCMAKE_FILE ${CMAKE_BINARY_DIR}/pd.cmake)
set(PDCMAKE_VERSION "v0.2.15")
if(NOT EXISTS ${PDCMAKE_FILE})
    message(STATUS "Downloading pd.cmake")
    file(DOWNLOAD https://raw.githubusercontent.com/pure-data/pd.cmake/refs/heads/main/pd.cmake ${PDCMAKE_FILE}
         STATUS DOWNLOAD_STATUS)
endif()
include(${PDCMAKE_FILE})

# CPM
set(CPM_VERSION "0.42.1")
set(CPM_FILE ${CMAKE_BINARY_DIR}/CPM.cmake)
if(NOT EXISTS "${CPM_FILE}")
    file(DOWNLOAD "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_VERSION}/CPM.cmake" ${CPM_FILE})
endif()
include(${CPM_FILE})

# OpenBLAS
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT EMSCRIPTEN)
    cpmaddpackage(
        NAME
        OpenBLAS
        VERSION
        0.3.33
        URL
        https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.33/OpenBLAS-0.3.33.tar.gz
        OPTIONS
        "BUILD_SHARED_LIBS OFF")
elseif(EMSCRIPTEN)
    set(OPENBLAS_FILE "${CMAKE_BINARY_DIR}/OpenBLAS.zip")
    set(OPENBLAS_SOURCE_DIR "${CMAKE_BINARY_DIR}/OpenBLAS") # Nome fixo da pasta extraída
    set(OPENBLAS_TEMP_DIR "${CMAKE_BINARY_DIR}/OpenBLAS-temp") # Pasta temporária

    # Baixar OpenBLAS se não existir
    if(NOT EXISTS ${OPENBLAS_FILE})
        message(STATUS "Downloading OpenBLAS...")
        file(
            DOWNLOAD https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.29/OpenBLAS-0.3.29.zip
            ${OPENBLAS_FILE}
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS)
    endif()

    # Extrair OpenBLAS se o diretório não existir
    if(NOT EXISTS ${OPENBLAS_SOURCE_DIR})
        message(STATUS "Extracting OpenBLAS...")
        if(WIN32)
            execute_process(COMMAND ${CMAKE_COMMAND} -E unzip ${OPENBLAS_FILE} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        else()
            execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${OPENBLAS_FILE} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        endif()
        file(GLOB EXTRACTED_DIR "${CMAKE_BINARY_DIR}/OpenBLAS-*")
        if(EXTRACTED_DIR)
            file(RENAME "${EXTRACTED_DIR}" "${OPENBLAS_SOURCE_DIR}")
        else()
            message(FATAL_ERROR "OpenBLAS extraction failed! Folder not found.")
        endif()
    endif()

    # Incluir diretórios necessários
    include_directories("${OPENBLAS_SOURCE_DIR}/lapack-netlib/CBLAS/include/")
    include_directories("${OPENBLAS_SOURCE_DIR}/lapack-netlib/LAPACKE/include/")

    # Gerar cabeçalho de manipulação do CBLAS
    set(CBLAS_MANGLING_HEADER "${CMAKE_BINARY_DIR}/cblas_mangling.h")
    file(WRITE "${CBLAS_MANGLING_HEADER}" "")

    set(LIB_OPENBLAS "${CMAKE_BINARY_DIR}/libopenblas.a")
    if(NOT EXISTS ${LIB_OPENBLAS})
        file(DOWNLOAD https://github.com/charlesneimog/pd4web/raw/refs/heads/main/Resources/Wasm/libopenblas.a
             ${LIB_OPENBLAS} STATUS DOWNLOAD_STATUS)
    endif()

    set(LAPACKE_LIBRARY "${LIB_OPENBLAS}")
    set(OPENBLAS_LIBRARY "${LIB_OPENBLAS}")
endif()

# FFTW3
cpmaddpackage(
    NAME
    fftw3
    URL
    https://www.fftw.org/fftw-3.3.11.tar.gz
    OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "CMAKE_POSITION_INDEPENDENT_CODE ON"
    "ENABLE_FLOAT OFF")

# ╭──────────────────────────────────────╮
# │                 SAF                  │
# ╰──────────────────────────────────────╯
set(SAF_PERFORMANCE_LIB SAF_USE_OPEN_BLAS_AND_LAPACKE)
cpmaddpackage("gh:charlesneimog/Spatial_Audio_Framework#emscripten")
target_include_directories(
    saf
    PRIVATE ${OpenBLAS_BINARY_DIR} # Provides the safe, generated cblas.h and openblas_config.h
            ${CMAKE_BINARY_DIR}/generated
            ${CMAKE_BINARY_DIR}/
            ${OpenBLAS_SOURCE_DIR}/lapack-netlib/LAPACKE/include # Provides static lapacke.h
            ${OpenBLAS_BINARY_DIR}/lapack-netlib/LAPACKE/include # Provides generated lapacke_mangling.h
)
target_include_directories(saf PUBLIC "${saf_SOURCE_DIR}/../examples/include/")
if(NOT APPLE AND NOT EMSCRIPTEN)
    target_link_options(saf INTERFACE -static-libgcc -static-libstdc++)
    target_link_libraries(saf PUBLIC fftw3 gfortran)
endif()

# ╭──────────────────────────────────────╮
# │              PD OBJECTS              │
# ╰──────────────────────────────────────╯
file(GLOB ENCODER_SRC "${saf_SOURCE_DIR}/../examples/src/ambi_enc/*.c")
pd_add_external(saf.encoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/encoder~.c;${ENCODER_SRC}" LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(GLOB PANNER_SRC "${saf_SOURCE_DIR}/../examples/src/panner/*.c")
pd_add_external(saf.panner~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/panner~.c;${PANNER_SRC}" LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(GLOB ROOMSIM_TILDE_SOURCE "${saf_SOURCE_DIR}/../examples/src/ambi_roomsim/*.c")

pd_add_external(saf.roomsim~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/roomsim~.c;${ROOMSIM_TILDE_SOURCE}" LINK_LIBRARIES
                saf)

# ──────────────────────────────────────
file(GLOB DECODER_SRC "${saf_SOURCE_DIR}/../examples/src/ambi_dec/*.c")
pd_add_external(saf.decoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/decoder~.c;${DECODER_SRC}" LINK_LIBRARIES saf)

# # ─────────────────────────────────────
file(GLOB BINAURAL_TILDE_SOURCE "${saf_SOURCE_DIR}/../examples/src/ambi_bin/*.c")

pd_add_external(saf.binaural~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binaural~.c;${BINAURAL_TILDE_SOURCE}" LINK_LIBRARIES
                saf)

# ─────────────────────────────────────
file(GLOB PITCHSHIFTER_TILDE_SOURCE "${saf_SOURCE_DIR}/examples/src/pitch_shifter/*.c")
pd_add_external(saf.pitchshifter~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/pitchshifter~.c;${PITCHSHIFTER_TILDE_SOURCE}"
                LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(GLOB CONVOLVER_TILDE_SOURCE "${saf_SOURCE_DIR}/../examples/src/multiconv/*.c")
pd_add_external(saf.multiconvolve~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/multiconvolve~.c;${CONVOLVER_TILDE_SOURCE}"
                LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(GLOB BINAURALIZER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${saf_SOURCE_DIR}/examples/src/binauraliser/*.c")
pd_add_external(saf.binauraliser~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binauraliser~.c;${BINAURALIZER_SOURCE}"
                LINK_LIBRARIES saf)

file(GLOB SLDOA_TILDE_SOURCE "${saf_SOURCE_DIR}/examples/src/sldoa/*.c")
pd_add_external(saf.sldoa~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/sldoa~.c;${SLDOA_TILDE_SOURCE}" LINK_LIBRARIES saf)

# ╭──────────────────────────────────────╮
# │              DATA FILES              │
# ╰──────────────────────────────────────╯
file(GLOB HELP_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.pd")
file(GLOB SOUNDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.wav")
file(GLOB LUA_OBJECTS "${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.pd_lua")
pd_add_datafile(saf.encoder~ "${HELP_PATCHES}")
pd_add_datafile(saf.encoder~ "${SOUNDS}")
pd_add_datafile(saf.encoder~ "${LUA_OBJECTS}")
