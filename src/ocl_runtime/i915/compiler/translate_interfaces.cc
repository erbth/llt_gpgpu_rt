#include <stdexcept>
#include "translate_interfaces.h"

using namespace std;

namespace OCL
{

PRODUCT_FAMILY get_product_family(const struct intel_device_info& dev_info)
{
	switch (dev_info.platform)
	{
	case INTEL_PLATFORM_BDW:
		return IGFX_BROADWELL;
	case INTEL_PLATFORM_CHV:
		return IGFX_CHERRYVIEW;
	case INTEL_PLATFORM_SKL:
		return IGFX_SKYLAKE;
	case INTEL_PLATFORM_BXT:
		return IGFX_BROXTON;
	case INTEL_PLATFORM_KBL:
		return IGFX_KABYLAKE;
	case INTEL_PLATFORM_GLK:
		return IGFX_GEMINILAKE;
	case INTEL_PLATFORM_CFL:
		return IGFX_COFFEELAKE;
	case INTEL_PLATFORM_EHL:
		return IGFX_ELKHARTLAKE;
	case INTEL_PLATFORM_TGL:
		return IGFX_TIGERLAKE_LP;
	case INTEL_PLATFORM_RKL:
		return IGFX_ROCKETLAKE;
	case INTEL_PLATFORM_DG1:
		return IGFX_DG1;
	default:
		return IGFX_UNKNOWN;
	}
}

enum intel_platform get_intel_platform(PRODUCT_FAMILY family)
{
	switch (family)
	{
	case IGFX_BROADWELL:
		return INTEL_PLATFORM_BDW;
	case IGFX_CHERRYVIEW:
		return INTEL_PLATFORM_CHV;
	case IGFX_SKYLAKE:
		return INTEL_PLATFORM_SKL;
	case IGFX_BROXTON:
		return INTEL_PLATFORM_BXT;
	case IGFX_KABYLAKE:
		return INTEL_PLATFORM_KBL;
	case IGFX_GEMINILAKE:
		return INTEL_PLATFORM_GLK;
	case IGFX_COFFEELAKE:
		return INTEL_PLATFORM_CFL;
	case IGFX_ELKHARTLAKE:
		return INTEL_PLATFORM_EHL;
	case IGFX_TIGERLAKE_LP:
		return INTEL_PLATFORM_TGL;
	case IGFX_ROCKETLAKE:
		return INTEL_PLATFORM_RKL;
	case IGFX_DG1:
		return INTEL_PLATFORM_DG1;
	default:
		throw invalid_argument("No translation to enum intel_platform known");
	}
}

// PCH_PRODUCT_FAMILY get_pch_product_family(dev_info)
// {
// 	return PCH_UNKNOWN;
// }
// 
// get_display_core_family(dev_info);

GFXCORE_FAMILY get_render_core_family(const struct intel_device_info& dev_info)
{
	switch (dev_info.verx10)
	{
	case 60:
		return IGFX_GEN6_CORE;

	case 70:
		return IGFX_GEN7_CORE;
	case 75:
		return IGFX_GEN7_5_CORE;

	case 80:
		return IGFX_GEN8_CORE;

	case 90:
		return IGFX_GEN9_CORE;

	case 100:
		return dev_info.lp ? IGFX_GEN10LP_CORE: IGFX_GEN10_CORE;

	case 110:
		return dev_info.lp ? IGFX_GEN11LP_CORE : IGFX_GEN11_CORE;

	case 120:
		return dev_info.lp ? IGFX_GEN12LP_CORE : IGFX_GEN12_CORE;

	default:
		return IGFX_UNKNOWN_CORE;
	}
}

// get_platform_type(dev_info);

//	switch (dev_info.platform)
//	{
//	case INTEL_PLATFORM_GFX3:
//	case INTEL_PLATFORM_I965:
//	case INTEL_PLATFORM_ILK:
//	case INTEL_PLATFORM_G4X:
//	case INTEL_PLATFORM_SNB:
//	case INTEL_PLATFORM_IVB:
//	case INTEL_PLATFORM_BYT:
//	case INTEL_PLATFORM_HSW:
//	case INTEL_PLATFORM_BDW:
//	case INTEL_PLATFORM_CHV:
//	case INTEL_PLATFORM_SKL:
//	case INTEL_PLATFORM_BXT:
//	case INTEL_PLATFORM_KBL:
//	case INTEL_PLATFORM_GLK:
//	case INTEL_PLATFORM_CFL:
//	case INTEL_PLATFORM_EHL:
//	case INTEL_PLATFORM_TGL:
//	case INTEL_PLATFORM_RKL:
//	case INTEL_PLATFORM_DG1:
//	case INTEL_PLATFORM_DG2_G10:
//	case INTEL_PLATFORM_DG2_G11:
//	case INTEL_PLATFORM_DG2_G12:
//	case INTEL_PLATFORM_MTL_M:
//	case INTEL_PLATFORM_MTL_P:
//	default:
//	}

}
