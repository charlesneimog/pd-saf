cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(saf)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ╭──────────────────────────────────────╮
# │               pd.cmake               │
# ╰──────────────────────────────────────╯
cmake_policy(SET CMP0135 NEW)
set(PDCMAKE_FILE ${CMAKE_BINARY_DIR}/pd.cmake)
if(NOT EXISTS ${PDCMAKE_FILE})
  message(STATUS "Downloading pd.cmake")
  file(
    DOWNLOAD
    https://raw.githubusercontent.com/pure-data/pd.cmake/refs/tags/v0.2.0/pd.cmake
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
set(BUILD_TESTING OFF)
set(BUILD_LAPACK_DEPRECATED OFF)
set(C_LAPACK OFF)
set(CBLAS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT EMSCRIPTEN)
  set(OPENBLAS_FILE "${CMAKE_BINARY_DIR}/OpenBLAS.zip")
  set(OPENBLAS_SOURCE_DIR "${CMAKE_BINARY_DIR}/OpenBLAS") # Nome fixo da pasta
                                                          # extraída
  set(OPENBLAS_TEMP_DIR "${CMAKE_BINARY_DIR}/OpenBLAS-temp") # Pasta temporária

  # Baixar OpenBLAS se não existir
  if(NOT EXISTS ${OPENBLAS_FILE})
    message(STATUS "Downloading OpenBLAS...")
    file(
      DOWNLOAD
      https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.29/OpenBLAS-0.3.29.zip
      ${OPENBLAS_FILE}
      SHOW_PROGRESS
      STATUS DOWNLOAD_STATUS)
  endif()

  # Extrair OpenBLAS se o diretório não existir
  if(NOT EXISTS ${OPENBLAS_SOURCE_DIR})
    message(STATUS "Extracting OpenBLAS...")

    if(WIN32)
      execute_process(COMMAND ${CMAKE_COMMAND} -E unzip ${OPENBLAS_FILE}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    else()
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${OPENBLAS_FILE}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
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
  add_subdirectory(${OPENBLAS_SOURCE_DIR} ${CMAKE_BINARY_DIR}/OpenBLAS
                   EXCLUDE_FROM_ALL)
  # Incluir diretórios necessários
  include_directories("${OPENBLAS_SOURCE_DIR}/lapack-netlib/CBLAS/include/")
  include_directories("${OPENBLAS_SOURCE_DIR}/lapack-netlib/LAPACKE/include/")
  set(CBLAS_MANGLING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/cblas_mangling.h")
  file(WRITE "${CBLAS_MANGLING_HEADER}" "")
  include_directories("${CMAKE_CURRENT_BINARY_DIR}/")
  set(LAPACKE_LIBRARY openblas)
  set(OPENBLAS_LIBRARY openblas)
elseif(EMSCRIPTEN)
  set(OPENBLAS_FILE "${CMAKE_BINARY_DIR}/OpenBLAS.zip")
  set(OPENBLAS_SOURCE_DIR "${CMAKE_BINARY_DIR}/OpenBLAS") # Nome fixo da pasta
                                                          # extraída
  set(OPENBLAS_TEMP_DIR "${CMAKE_BINARY_DIR}/OpenBLAS-temp") # Pasta temporária

  # Baixar OpenBLAS se não existir
  if(NOT EXISTS ${OPENBLAS_FILE})
    message(STATUS "Downloading OpenBLAS...")
    file(
      DOWNLOAD
      https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.29/OpenBLAS-0.3.29.zip
      ${OPENBLAS_FILE}
      SHOW_PROGRESS
      STATUS DOWNLOAD_STATUS)
  endif()

  # Extrair OpenBLAS se o diretório não existir
  if(NOT EXISTS ${OPENBLAS_SOURCE_DIR})
    message(STATUS "Extracting OpenBLAS...")
    if(WIN32)
      execute_process(COMMAND ${CMAKE_COMMAND} -E unzip ${OPENBLAS_FILE}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    else()
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${OPENBLAS_FILE}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
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
  set(CBLAS_MANGLING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/cblas_mangling.h")
  file(WRITE "${CBLAS_MANGLING_HEADER}" "")

  set(LIB_OPENBLAS "${CMAKE_BINARY_DIR}/libopenblas.a")
  if(NOT EXISTS ${LIB_OPENBLAS})
    file(
      DOWNLOAD
      https://github.com/charlesneimog/pd4web/raw/refs/heads/main/Resources/Wasm/libopenblas.a
      ${LIB_OPENBLAS}
      SHOW_PROGRESS
      STATUS DOWNLOAD_STATUS)
  endif()

  set(LAPACKE_LIBRARY "${LIB_OPENBLAS}")
  set(OPENBLAS_LIBRARY "${LIB_OPENBLAS}")
endif()

# ╭──────────────────────────────────────╮
# │                FFTW3                 │
# ╰──────────────────────────────────────╯
if(NOT TARGET fftw3f)
  cmake_policy(SET CMP0135 NEW)
  option(BUILD_SHARED_LIBS OFF)
  option(BUILD_TESTS OFF)
  option(ENABLE_FLOAT ON)

  set(FFTW3_FILE ${CMAKE_BINARY_DIR}/fftw-3.3.10.tar.gz)
  if(NOT EXISTS ${FFTW3_FILE})
    message(STATUS "Downloading FFTW3")
    file(
      DOWNLOAD https://www.fftw.org/fftw-3.3.10.tar.gz ${FFTW3_FILE}
      SHOW_PROGRESS
      STATUS DOWNLOAD_STATUS)
  endif()

  file(ARCHIVE_EXTRACT INPUT ${CMAKE_BINARY_DIR}/fftw-3.3.10.tar.gz DESTINATION
       ${CMAKE_BINARY_DIR}/)

  add_subdirectory(${CMAKE_BINARY_DIR}/fftw-3.3.10 ${CMAKE_BINARY_DIR}/fftw3f
                   EXCLUDE_FROM_ALL)
  set_target_properties(fftw3f PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                          ${CMAKE_SOURCE_DIR})
  set_target_properties(fftw3f PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
  set(SAF_ENABLE_SIMD ON)
  set(SAF_PERFORMANCE_LIB SAF_USE_OPEN_BLAS_AND_LAPACKE)
  add_definitions(-DSAF_USE_OPEN_BLAS_AND_LAPACKE)
  set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgfortran -static-libgcc")
else()
  set(SAF_PERFORMANCE_LIB "SAF_USE_APPLE_ACCELERATE")
endif()

add_subdirectory(Libraries/Spatial_Audio_Framework
                 ${CMAKE_BINARY_DIR}/Spatial_Audio_Framework EXCLUDE_FROM_ALL)
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/include"
)
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/framework/include/"
)

if(NOT APPLE AND NOT EMSCRIPTEN)
  target_link_libraries(saf PUBLIC gfortran fftw3f)
endif()

# ╭──────────────────────────────────────╮
# │              PD OBJECTS              │
# ╰──────────────────────────────────────╯
file(
  GLOB
  ENCODER_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_enc/*.c"
)

pd_add_external(
  saf.encoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/encoder~.c;${ENCODER_SRC}"
  LINK_LIBRARIES saf fftw3f)

# ──────────────────────────────────────
file(
  GLOB
  saf_dec_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_dec/*.c"
)
pd_add_external(
  saf.decoder~ "${CMAKE_CURRENT_SOURCE_DIR}/Sources/decoder~.c;${saf_dec_SRC}"
  LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(
  GLOB
  BINAURAL_TILDE_SOURCE
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_bin/*.c"
)
pd_add_external(
  saf.binaural~
  "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binaural~.c;${BINAURAL_TILDE_SOURCE}"
  LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(
  GLOB
  ROOMSIM_TILDE_SOURCE
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/ambi_roomsim/*.c"
)

pd_add_external(
  saf.roomsim~
  "${CMAKE_CURRENT_SOURCE_DIR}/Sources/roomsim~.c;${ROOMSIM_TILDE_SOURCE}"
  LINK_LIBRARIES saf)

# ─────────────────────────────────────
file(
  GLOB
  PITCHSHIFTER_TILDE_SOURCE
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/pitch_shifter/*.c"
)

pd_add_external(
  saf.pitchshifter~
  "${CMAKE_CURRENT_SOURCE_DIR}/Sources/pitchshifter~.c;${PITCHSHIFTER_TILDE_SOURCE}"
  LINK_LIBRARIES
  saf)

# ─────────────────────────────────────
file(
  GLOB
  BINAURALIZER_SOURCE
  "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/examples/src/binauraliser/*.c"
)

pd_add_external(
  saf.binauraliser~
  "${CMAKE_CURRENT_SOURCE_DIR}/Sources/binauraliser~.c;${BINAURALIZER_SOURCE}"
  LINK_LIBRARIES saf)

pd_add_external(pdsaf "${CMAKE_CURRENT_SOURCE_DIR}/Sources/saf.c"
                LINK_LIBRARIES saf EXTERNAL_NAME saf)

# ╭──────────────────────────────────────╮
# │              DATA FILES              │
# ╰──────────────────────────────────────╯
file(GLOB HELP_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.pd")
file(GLOB SOUNDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.wav")
file(GLOB LUA_OBJECTS "${CMAKE_CURRENT_SOURCE_DIR}/Sources/*.pd_lua")
pd_add_datafile(saf.binaural~ "${HELP_PATCHES}")
pd_add_datafile(saf.binaural~ "${SOUNDS}")
pd_add_datafile(saf.binaural~ "${LUA_OBJECTS}")
