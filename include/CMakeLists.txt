configure_file(llt_gpgpu_rt/version.h.in llt_gpgpu_rt/version.h)

install(
	FILES
		llt_gpgpu_rt/ocl_runtime.h
		llt_gpgpu_rt/i915_runtime.h
		llt_gpgpu_rt/i915_compiled_program.h
		llt_gpgpu_rt/i915_device.h
		"${CMAKE_CURRENT_BINARY_DIR}/llt_gpgpu_rt/version.h"
	DESTINATION
		include/llt_gpgpu_rt/)
