#ifndef __OFFLINE_DEVICE_INFO_H
#define __OFFLINE_DEVICE_INFO_H

#include <cstdint>
#include <map>
#include <igfxfmid.h>

struct OfflineDeviceInfo
{
	PRODUCT_FAMILY product_family;
	GFXCORE_FAMILY render_core_family;
	uint64_t default_timestamp_frequency;

	inline OfflineDeviceInfo(PRODUCT_FAMILY product_family,
			GFXCORE_FAMILY render_core_family,
			uint64_t default_timestamp_frequency)
		:
			product_family(product_family),
			render_core_family(render_core_family),
			default_timestamp_frequency(default_timestamp_frequency)
	{
	}
};

std::map<PRODUCT_FAMILY, OfflineDeviceInfo> get_product_family_map();

#endif /* __OFFLINE_DEVICE_INFO_H */
