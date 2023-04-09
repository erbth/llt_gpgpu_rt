#include <stdexcept>
#include "i915_device_translate.h"

using namespace std;

namespace OCL
{

i915_device_type intel_platform_to_device_type(enum intel_platform platform)
{
	/* NOTE: llt_gpgpu_rt_i915_c assumes that there is exactly on device type
	 * for each platform. */
	switch (platform)
	{
	case INTEL_PLATFORM_BDW:
		return I915_DEVICE_TYPE_BDW;
	case INTEL_PLATFORM_CHV:
		return I915_DEVICE_TYPE_CHV;
	case INTEL_PLATFORM_SKL:
		return I915_DEVICE_TYPE_SKL;
	case INTEL_PLATFORM_BXT:
		return I915_DEVICE_TYPE_BXT;
	case INTEL_PLATFORM_KBL:
		return I915_DEVICE_TYPE_KBL;
	case INTEL_PLATFORM_GLK:
		return I915_DEVICE_TYPE_GLK;
	case INTEL_PLATFORM_CFL:
		return I915_DEVICE_TYPE_CFL;
	case INTEL_PLATFORM_EHL:
		return I915_DEVICE_TYPE_EHL;
	case INTEL_PLATFORM_TGL:
		return I915_DEVICE_TYPE_TGL;
	case INTEL_PLATFORM_RKL:
		return I915_DEVICE_TYPE_RKL;
	case INTEL_PLATFORM_DG1:
		return I915_DEVICE_TYPE_DG1;
	default:
		throw invalid_argument("Unknown intel_platform");
	}
}

}
