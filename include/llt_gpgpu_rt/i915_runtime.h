/** Heavily inspired by OpenCL */
#ifndef __LLT_GPGPU_RT_I915_RUNTIME_H
#define __LLT_GPGPU_RT_I915_RUNTIME_H

#include <string>
#include <memory>
#include <llt_gpgpu_rt/ocl_runtime.h>
#include <llt_gpgpu_rt/i915_compiled_program.h>

extern "C" {
#include <xf86drm.h>
}

namespace OCL
{

class I915Kernel : public Kernel
{
public:
	~I915Kernel() = 0;
};

class I915PreparedKernel : public PreparedKernel
{
public:
	~I915PreparedKernel() = 0;

	/* @param size is in bytes */
	virtual void add_argument_gem_name(uint32_t name) = 0;
};

class I915RTE : public RTE
{
public:
	virtual ~I915RTE() = 0;

	/* For offline compiled kernels */
	virtual std::shared_ptr<Kernel> read_compiled_kernel(
			const I915CompiledProgram& program, const char* name) = 0;

	virtual size_t get_page_size() = 0;
	virtual drm_magic_t get_drm_magic() = 0;
};

std::unique_ptr<I915RTE> create_i915_rte(const char* device);

}

#endif /* __LLT_GPGPU_RT_I915_RUNTIME_H */
