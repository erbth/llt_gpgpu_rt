#ifndef __LLT_GPGPU_RT_I915_COMPILED_PROGRAM_H
#define __LLT_GPGPU_RT_I915_COMPILED_PROGRAM_H

#include <utility>
#include <optional>
#include <llt_gpgpu_rt/i915_device.h>

namespace OCL
{

/* Base class for offline compiled programs */
class I915CompiledProgram
{
public:
	virtual ~I915CompiledProgram() = 0;

	virtual std::optional<std::pair<const char*, size_t>>
		get_bin(i915_device_type device_type) const = 0;
};

}

#endif /* __LLT_GPGPU_RT_I915_COMPILED_PROGRAM_H */
