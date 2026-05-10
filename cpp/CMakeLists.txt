# ==============================================================================
# CMakeLists.txt
# Build configuration for the UV unwrapping library
#
# Provided by Mixar.
# ==============================================================================

cmake_minimum_required(VERSION 3.15)
project(uvunwrap)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fix MSVC "Unsafe" Warnings ---
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
endif()

# --- Dependencies ---
find_package(Eigen3 3.3 QUIET)
if(EIGEN3_FOUND)
    include_directories(${EIGEN3_INCLUDE_DIR})
    message(STATUS "Using system Eigen: ${EIGEN3_INCLUDE_DIR}")
else()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen)
        message(STATUS "Using bundled Eigen")
    else()
        # Fallback paths for Windows
        if(EXISTS "C:/Program Files/Eigen3/include/eigen3")
            include_directories("C:/Program Files/Eigen3/include/eigen3")
        elseif(EXISTS "C:/local/eigen-3.4.0")
            include_directories("C:/local/eigen-3.4.0")
        else()
            message(STATUS "Eigen not found via find_package. Assuming standard path.")
        endif()
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SOURCES
    src/mesh_io.cpp
    src/math_utils.cpp
    src/topology.cpp
    src/seam_detection.cpp
    src/lscm.cpp
    src/packing.cpp
    src/unwrap.cpp
)

# --- Main Library ---
add_library(uvunwrap SHARED ${SOURCES})

set_target_properties(uvunwrap PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# --- Test Executable ---
add_executable(test_unwrap tests/test_unwrap.cpp)
target_link_libraries(test_unwrap PRIVATE uvunwrap)

# --- Compiler Options ---
if(MSVC)
    target_compile_options(uvunwrap PRIVATE /W4)
else()
    target_compile_options(uvunwrap PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Auto-Copy to Blender (Optional but Recommended) ---
if(MSVC)
    add_custom_command(TARGET uvunwrap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:uvunwrap>
        "${CMAKE_SOURCE_DIR}/../part3_blender/autouv/uvwrap/$<TARGET_FILE_NAME:uvunwrap>"
        COMMENT "Copying DLL to Blender folder..."
    )
endif()