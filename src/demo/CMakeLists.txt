add_executable(i915_memset
	i915_memset.cc)

target_include_directories(i915_memset PRIVATE ocl_runtime_i915)
target_link_libraries(i915_memset ocl_runtime_i915)



if (COMPILE_X_DEMOS)

add_executable(i915_xcb_display
	i915_xcb_display.cc)

target_include_directories(i915_xcb_display PRIVATE
	ocl_runtime_i915
	${XCB_INCLUDE_DIRS})

target_link_libraries(i915_xcb_display PRIVATE
	ocl_runtime_i915
	${XCB_LIBRARIES})

endif()
