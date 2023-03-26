# Runtime library
add_library(ocl_runtime_i915
	i915_runtime.cc
	i915_utils.cc
	i915_kernel_utils.cc
	igc_progbin.cc
	hash.cc
	utils.cc
	compiler/igc_interface.cc
	third_party/mesa/intel_device_info.c
	third_party/mesa/intel_hwconfig.c
	../ocl_runtime.cc
)

target_include_directories(ocl_runtime_i915
	PRIVATE
		${IGC_INCLUDE_DIRS}
		${IGDGMM_INCLUDE_DIRS}
		"${CMAKE_CURRENT_BINARY_DIR}"
	PUBLIC
		${LIBDRM_INCLUDE_DIRS}
		"${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ocl_runtime_i915 PRIVATE
	${LIBDRM_LIBRARIES}
	${IGC_LIBRARIES}
	-ldl)

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
