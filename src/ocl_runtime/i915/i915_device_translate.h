#ifndef __I915_DEVICE_TRANSLATE_H
#define __I915_DEVICE_TRANSLATE_H

#include <llt_gpgpu_rt/i915_device.h>
#include "third_party/mesa/intel_device_info.h"

namespace OCL
{

i915_device_type intel_platform_to_device_type(enum intel_platform platform);

}

#endif /* __I915_DEVICE_TRANSLATE_H */
