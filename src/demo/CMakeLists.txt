llt_gpgpu_compile_i915(i915_memset.clch i915_memset.cl)
add_executable(i915_memset
	i915_memset.cc
	i915_memset.clch)

target_include_directories(i915_memset PRIVATE
	llt_gpgpu_rt_i915
	"${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(i915_memset llt_gpgpu_rt_i915)


llt_gpgpu_compile_i915(i915_memset_slm.clch i915_memset_slm.cl)
add_executable(i915_memset_slm
	i915_memset_slm.cc
	i915_memset_slm.clch)

target_include_directories(i915_memset_slm PRIVATE
	llt_gpgpu_rt_i915
	"${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(i915_memset_slm llt_gpgpu_rt_i915)


if (ENABLE_ONLINE_COMPILER)

add_executable(i915_memset_online_compiled
	i915_memset_online_compiled.cc)

target_include_directories(i915_memset_online_compiled PRIVATE llt_gpgpu_rt_i915)
target_link_libraries(i915_memset_online_compiled llt_gpgpu_rt_i915)

endif()


if (COMPILE_X_DEMOS)

llt_gpgpu_compile_i915(i915_xcb_display.clch i915_xcb_display.cl)
add_executable(i915_xcb_display
	i915_xcb_display.cc
	i915_xcb_display.clch)

target_include_directories(i915_xcb_display PRIVATE
	llt_gpgpu_rt_i915
	${XCB_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(i915_xcb_display PRIVATE
	llt_gpgpu_rt_i915
	${XCB_LIBRARIES})

endif()
