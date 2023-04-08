/** Keep consistent with third_party/mesa/intel_device_info.c */
#include <cstring>
#include <stdexcept>
#include "offline_device_info.h"

using namespace std;

/* Available codenames and codename -> gen version adapted from
 * intel_device_info.c; default timestamp frequencies for Gen11 and newer from
 * intel-compute-runtime. */
map<PRODUCT_FAMILY, OfflineDeviceInfo> get_product_family_map()
{
	map<PRODUCT_FAMILY, OfflineDeviceInfo> m;

	m.insert({
			IGFX_BROADWELL,
			OfflineDeviceInfo(
					IGFX_BROADWELL,
					IGFX_GEN8_CORE,
					12500000)
	});

	m.insert({
			IGFX_CHERRYVIEW,
			OfflineDeviceInfo(
					IGFX_CHERRYVIEW,
					IGFX_GEN8_CORE,
					12500000)
	});

	m.insert({
			IGFX_SKYLAKE,
			OfflineDeviceInfo(
					IGFX_SKYLAKE,
					IGFX_GEN9_CORE,
					12000000)
	});

	m.insert({
			IGFX_BROXTON,
			OfflineDeviceInfo(
					IGFX_BROXTON,
					IGFX_GEN9_CORE,
					19200000)
	});

	m.insert({
			IGFX_KABYLAKE,
			OfflineDeviceInfo(
					IGFX_KABYLAKE,
					IGFX_GEN9_CORE,
					12000000)
	});

	m.insert({
			IGFX_GEMINILAKE,
			OfflineDeviceInfo(
					IGFX_GEMINILAKE,
					IGFX_GEN9_CORE,
					19200000)
	});

	m.insert({
			IGFX_COFFEELAKE,
			OfflineDeviceInfo(
					IGFX_COFFEELAKE,
					IGFX_GEN9_CORE,
					12000000)
	});

	m.insert({
			IGFX_ELKHARTLAKE,
			OfflineDeviceInfo(
					IGFX_ELKHARTLAKE,
					IGFX_GEN11LP_CORE,
					12000000),
	});

	/* NOTE: in intel_device_info.c the lp-flag is not set for Tigerlake. Hence
	 * use core family GEN12_CORE (without lp) here to be consistent with the
	 * runtime-compiler, which relies on intel_device_info */
	m.insert({
			IGFX_TIGERLAKE_LP,
			OfflineDeviceInfo(
					IGFX_TIGERLAKE_LP,
					IGFX_GEN12_CORE,
					12000000)
	
	});

	m.insert({
			IGFX_ROCKETLAKE,
			OfflineDeviceInfo(
					IGFX_ROCKETLAKE,
					IGFX_GEN12_CORE,
					12000000)
	});

	m.insert({
			IGFX_DG1,
			OfflineDeviceInfo(
					IGFX_DG1,
					IGFX_GEN12_CORE,
					12000000)
	});

	return m;
}
