add_executable(llt_gpgpu_rt_i915_c
	igc_interface.cc
	offline_device_info.cc
	llt_gpgpu_rt_i915_c.cc
	translate_interfaces.cc
)

target_include_directories(llt_gpgpu_rt_i915_c PRIVATE
	${IGC_INCLUDE_DIRS}
	${IGDGMM_INCLUDE_DIRS})

target_link_libraries(llt_gpgpu_rt_i915_c PRIVATE
	${IGC_LIBRARIES}
	-ldl)
