#ifndef __I915_COMPILED_PROGRAM_H
#define __I915_COMPILED_PROGRAM_H

#include <utility>
#include <optional>

#include "third_party/mesa/intel_device_info.h"

namespace OCL
{

/* Base class for offline compiled programs */
class I915CompiledProgram
{
public:
	virtual ~I915CompiledProgram() = 0;

	virtual std::optional<std::pair<const char*, size_t>>
		get_bin(enum intel_platform platform) const = 0;
};

}

#endif /* __I915_COMPILED_PROGRAM_H */
