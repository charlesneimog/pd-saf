# Define your standard CMake header (for example):
cmake_minimum_required(VERSION 3.25)
include(${CMAKE_CURRENT_SOURCE_DIR}/Resources/pd.build/pd.cmake)
project(pd-saf)

set_pd_external_path(${CMAKE_CURRENT_SOURCE_DIR}/pd-saf)

set(PD_FLOATSIZE64 "OFF")
#╭──────────────────────────────────────╮
#│           spatialaudioLib            │
#╰──────────────────────────────────────╯
cmake_policy(SET CMP0077 NEW)
set(SAF_BUILD_EXAMPLES OFF)
set(SAF_BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework)

#╭──────────────────────────────────────╮
#│             Pd External              │
#╰──────────────────────────────────────╯
file(GLOB EXTERNAL_SOURCES "${CMAKE_SOURCE_DIR}/Sources/*.cpp")
add_pd_external(pd-saf saf ${EXTERNAL_SOURCES})
target_include_directories(saf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/Spatial_Audio_Framework/framework/include")
target_link_libraries(pd-saf saf_head)

set_target_properties(pd-saf PROPERTIES CXX_STANDARD 11)
# target_compile_options(elseambi PRIVATE 
#                 "-O3" 
#                 "-funroll-loops" 
#                 "-fomit-frame-pointer" 
#                 "-mfpmath=sse" 
#                 "-msse" 
#                 "-msse2" 
#                 "-msse3" 
#                 "-DPD"
# )
