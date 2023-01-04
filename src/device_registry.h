#ifndef __DEVICE_REGISTRY_H
#define __DEVICE_REGISTRY_H

#include <cstdint>
#include <memory>
#include "hw_info.h"

struct DeviceDescription
{
	uint32_t device_id = 0;
	const char* name = nullptr;
	NEO::HardwareInfo hw_info{};

	DeviceDescription() = default;
	DeviceDescription(uint32_t device_id, const char* name,
			const NEO::HardwareInfo& hw_info);
};


DeviceDescription lookup_device_id(uint32_t device_id, uint32_t device_revision, uint64_t hw_config);

#endif /* __DEVICE_REGISTRY_H */
