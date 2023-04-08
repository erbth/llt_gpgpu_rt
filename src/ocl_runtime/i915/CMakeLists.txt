# Runtime library
set(OCL_RUNTIME_I915_SRC
	i915_runtime.cc
	i915_utils.cc
	i915_kernel_utils.cc
	i915_compiled_program.cc
	igc_progbin.cc
	hash.cc
	utils.cc
	third_party/mesa/intel_device_info.c
	third_party/mesa/intel_hwconfig.c
	../ocl_runtime.cc)

set(OCL_RUNTIME_I915_REQ_INCLUDE_DIRS
	${IGC_INCLUDE_DIRS}
	${IGDGMM_INCLUDE_DIRS}
	"${CMAKE_CURRENT_BINARY_DIR}")

set(OCL_RUNTIME_I915_REQ_LIBRARIES ${LIBDRM_LIBRARIES})

if (ENABLE_ONLINE_COMPILER)
	set(OCL_RUNTIME_I915_SRC ${OCL_RUNTIME_I915_SRC}
		compiler/igc_interface.cc
		compiler/translate_interfaces.cc)

	set(OCL_RUNTIME_I915_REQ_LIBRARIES ${OCL_RUNTIME_I915_REQ_LIBRARIES}
		${IGC_LIBRARIES}
		-ldl)
endif()


add_library(ocl_runtime_i915 ${OCL_RUNTIME_I915_SRC})

target_include_directories(ocl_runtime_i915
	PRIVATE
		${OCL_RUNTIME_I915_REQ_INCLUDE_DIRS}
	PUBLIC
		${LIBDRM_INCLUDE_DIRS}
		"${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ocl_runtime_i915 PRIVATE ${OCL_RUNTIME_I915_REQ_LIBRARIES})


# Generate hw interface header files from XML files
set(XML_FILES
	gen45.xml
	gen4.xml
	gen5.xml
	gen6.xml
	gen75.xml
	gen7.xml
	gen8.xml
	gen9.xml
	gen11.xml
	gen125.xml
	gen12.xml)

foreach(XML_FILE ${XML_FILES})
	set(CMD "${CMAKE_SOURCE_DIR}/tools/generate_gen_headers.py")
	set(XML_HDR_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mesa/xml/${XML_FILE}")

	get_filename_component(XML_FILE_PART "${XML_FILE}" NAME_WLE)
	set(XML_HDR_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${XML_FILE_PART}_hw_int.h")

	add_custom_command(
		OUTPUT "${XML_HDR_OUTPUT}"
		COMMAND "${CMD}" "${XML_HDR_INPUT}" "${XML_HDR_OUTPUT}"
		DEPENDS "${XML_HDR_INPUT}" "${CMD}")

	target_sources(ocl_runtime_i915 PRIVATE "${XML_HDR_OUTPUT}")
endforeach()


add_subdirectory(compiler)
