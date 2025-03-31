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
set(BUILD_STATIC_LIBS ON)
set(BUILD_WITHOUT_LAPACK OFF)
set(BUILD_TESTING OFF)
set(BUILD_LAPACK_DEPRECATED OFF)
set(C_LAPACK ON)
set(CBLAS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "APPLE")
    set(OPENBLAS_FILE "${CMAKE_BINARY_DIR}/OpenBLAS.zip")
    set(OPENBLAS_SOURCE_DIR "${CMAKE_BINARY_DIR}/OpenBLAS") # Nome fixo da pasta extraída
    set(OPENBLAS_TEMP_DIR "${CMAKE_BINARY_DIR}/OpenBLAS-temp") # Pasta temporária

    # Baixar OpenBLAS se não existir
    if(NOT EXISTS ${OPENBLAS_FILE})
        message(STATUS "Downloading OpenBLAS...")
        file(
            DOWNLOAD https://github.com/OpenMathLib/OpenBLAS/archive/8795fc7985635de1ecf674b87e2008a15097ffab.zip
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

        # Encontrar a pasta extraída (deve ser "OpenBLAS-<commit-hash>")
        file(GLOB EXTRACTED_DIR "${CMAKE_BINARY_DIR}/OpenBLAS-*")

        # Renomear para o nome fixo
        if(EXTRACTED_DIR)
            file(RENAME "${EXTRACTED_DIR}" "${OPENBLAS_SOURCE_DIR}")
        else()
            message(FATAL_ERROR "OpenBLAS extraction failed! Folder not found.")
        endif()
    endif()

    # Adicionar OpenBLAS ao projeto
    add_subdirectory(${OPENBLAS_SOURCE_DIR} EXCLUDE_FROM_ALL)

    # Incluir diretórios necessários
    include_directories("${OPENBLAS_SOURCE_DIR}/lapack-netlib/CBLAS/include/")
    include_directories("${OPENBLAS_SOURCE_DIR}/lapack-netlib/LAPACKE/include/")

    # Gerar cabeçalho de manipulação do CBLAS
    set(CBLAS_MANGLING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/cblas_mangling.h")
    file(WRITE "${CBLAS_MANGLING_HEADER}" "")

    include_directories("${CMAKE_CURRENT_BINARY_DIR}/")

    # Definir bibliotecas
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)
endif()

# ╭──────────────────────────────────────╮
# │              PD OBJECTS              │
# ╰──────────────────────────────────────╯
file(GLOB ENCODER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_enc/*.c")

pd_add_external(saf.encoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/encoder~.c;${ENCODER_SRC}" LINK_LIBRARIES saf)

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

pd_add_external(saf.pitchshifter~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/pitchshifter~.c;${PITCHSHIFTER_TILDE_SOURCE}"
                LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(GLOB BINAURALIZER_SOURCE
     "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/binauraliser/*.c")

pd_add_external(saf.binauraliser~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binauraliser~.c;${BINAURALIZER_SOURCE}"
                LINK_LIBRARIES saf)

# ╭──────────────────────────────────────╮
# │              DATA FILES              │
# ╰──────────────────────────────────────╯
file(GLOB HELP_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.pd")
file(GLOB SOUNDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.wav")
file(GLOB LUA_OBJECTS "${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.pd_lua")
pd_add_datafile(saf.binaural~ "${HELP_PATCHES}")
pd_add_datafile(saf.binaural~ "${SOUNDS}")
pd_add_datafile(saf.binaural~ "${LUA_OBJECTS}")
