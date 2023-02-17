cmake_minimum_required(VERSION 3.13)

project(i915_bare_gpu CXX)

# Require GCC
if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	message(FATAL_ERROR "Compiler is ${CMAKE_CXX_COMPILER_ID}, currently only GCC is supported (s.t. e.g. type puning through unions works safely)")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -gdwarf-2 -O3 -Wall")

# Find required packages
find_package(PkgConfig)
pkg_check_modules(LIBDRM REQUIRED libdrm libdrm_intel)
pkg_check_modules(IGC REQUIRED igc-opencl)

# Provides structs and constants for device families
pkg_check_modules(IGDGMM REQUIRED igdgmm)

add_subdirectory(src)
