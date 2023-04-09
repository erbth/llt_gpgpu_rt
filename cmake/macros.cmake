# NOTE: When changing this, change the deployed version in
# LltGpgpuRtConfig.cmake, too. There are two versions because the internal
# version has to depend on the compiler's target, while the deployed version has
# to find the compiler installed on the system.
function(llt_gpgpu_compile_i915 CL_TARGET CL_SRC)
	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CL_TARGET}"
		COMMAND
			llt_gpgpu_rt_i915_c -cl-std=CL1.2
				-o "${CMAKE_CURRENT_BINARY_DIR}/${CL_TARGET}"
				"${CMAKE_CURRENT_SOURCE_DIR}/${CL_SRC}"
		DEPENDS
			llt_gpgpu_rt_i915_c
			"${CMAKE_CURRENT_SOURCE_DIR}/${CL_SRC}")
endfunction()
