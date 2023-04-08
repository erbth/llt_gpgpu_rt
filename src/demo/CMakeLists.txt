add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/i915_memset.clch"
	COMMAND
		llt_gpgpu_rt_i915_c -cl-std=CL1.2
			-o "${CMAKE_CURRENT_BINARY_DIR}/i915_memset.clch"
			"${CMAKE_CURRENT_SOURCE_DIR}/i915_memset.cl"
	DEPENDS
		llt_gpgpu_rt_i915_c
		"${CMAKE_CURRENT_SOURCE_DIR}/i915_memset.cl")

add_executable(i915_memset
	i915_memset.cc
	"${CMAKE_CURRENT_BINARY_DIR}/i915_memset.clch")

target_include_directories(i915_memset PRIVATE
	ocl_runtime_i915
	"${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(i915_memset ocl_runtime_i915)


if (ENABLE_ONLINE_COMPILER)

add_executable(i915_memset_online_compiled
	i915_memset_online_compiled.cc)

target_include_directories(i915_memset_online_compiled PRIVATE ocl_runtime_i915)
target_link_libraries(i915_memset_online_compiled ocl_runtime_i915)

endif()


if (COMPILE_X_DEMOS)

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/i915_xcb_display.clch"
	COMMAND
		llt_gpgpu_rt_i915_c -cl-std=CL1.2
			-o "${CMAKE_CURRENT_BINARY_DIR}/i915_xcb_display.clch"
			"${CMAKE_CURRENT_SOURCE_DIR}/i915_xcb_display.cl"
	DEPENDS
		llt_gpgpu_rt_i915_c
		"${CMAKE_CURRENT_SOURCE_DIR}/i915_xcb_display.cl")

add_executable(i915_xcb_display
	i915_xcb_display.cc
	i915_xcb_display.clch)

target_include_directories(i915_xcb_display PRIVATE
	ocl_runtime_i915
	${XCB_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(i915_xcb_display PRIVATE
	ocl_runtime_i915
	${XCB_LIBRARIES})

endif()
