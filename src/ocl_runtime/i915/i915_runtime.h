/** Heavily inspired by OpenCL */
#ifndef __I915_RUNTIME_H
#define __I915_RUNTIME_H

#include <string>
#include <memory>
#include "../ocl_runtime.h"

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
};

class I915RTE : public RTE
{
public:
	virtual ~I915RTE() = 0;

	virtual size_t get_page_size() = 0;
};

std::unique_ptr<I915RTE> create_i915_rte(const char* device);

}

#endif /* __I915_RUNTIME_H */
