cmake_minimum_required(VERSION 3.13)

project(llt_gpgpu_rt C CXX)

# Project version
set(LLT_GPGPU_RT_VERSION_MAJOR 1)
set(LLT_GPGPU_RT_VERSION_MINOR 0)
set(LLT_GPGPU_RT_VERSION_PATCH 0)

set(LLT_GPGPU_RT_VERSION
	"${LLT_GPGPU_RT_VERSION_MAJOR}.${LLT_GPGPU_RT_VERSION_MINOR}.${LLT_GPGPU_RT_VERSION_PATCH}")

# Options
option(ENABLE_ONLINE_COMPILER "Enable online IGC compiler" OFF)

# Require GCC
if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	message(FATAL_ERROR "Compiler is ${CMAKE_CXX_COMPILER_ID}, currently only GCC is supported (s.t. e.g. type puning through unions works safely)")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -gdwarf-2 -O3 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -gdwarf-2 -O3 -Wall")

# Find required packages
find_package(PkgConfig)
pkg_check_modules(LIBDRM REQUIRED libdrm libdrm_intel)
pkg_check_modules(IGC REQUIRED igc-opencl)

# Provides structs and constants for device families
pkg_check_modules(IGDGMM REQUIRED igdgmm)

# Optional dependencies
pkg_check_modules(XCB xcb xcb-dri2)

if (XCB_FOUND)
	set(COMPILE_X_DEMOS TRUE)
endif()

# Include macros
include("cmake/macros.cmake")

# Configure a config file
configure_file(llt_gpgpu_rt_config.h.in llt_gpgpu_rt_config.h)
include_directories("${CMAKE_BINARY_DIR}")

# Add /include to search path
include_directories("${CMAKE_SOURCE_DIR}/include")

add_subdirectory(src)

# For installing public headers
add_subdirectory(include)


# Generate cmake config package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/LltGpgpuRtConfigVersion.cmake"
	VERSION "${LLT_GPGPU_RT_VERSION}"
	COMPATIBILITY SameMinorVersion)

if (NOT ("${CMAKE_INSTALL_PREFIX}/include" IN_LIST CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES))
	set(LLT_GPGPU_RT_INSTALLED_PACKAGE_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include")
else()
	set(LLT_GPGPU_RT_INSTALLED_PACKAGE_INCLUDE_DIRS "")
endif()

set(LLT_GPGPU_RT_INSTALLED_PACKAGE_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/libllt_gpgpu_rt_i915.a")
if (ENABLE_ONLINE_COMPILER)
	set(LLT_GPGPU_RT_INSTALLED_PACKAGE_LIBRARIES ${LLT_GPGPU_RT_INSTALLED_PACKAGE_LIBRARIES} -ldl)
endif()

configure_file(cmake/LltGpgpuRtConfig.cmake.in LltGpgpuRtConfig.cmake @ONLY)

install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/LltGpgpuRtConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/LltGpgpuRtConfigVersion.cmake"
	DESTINATION
		lib/cmake/LltGpgpuRt/)
