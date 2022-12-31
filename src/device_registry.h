#ifndef __DEVICE_REGISTRY_H
#define __DEVICE_REGISTRY_H

#include <cstdint>
#include <memory>

struct DeviceDescription
{
	uint32_t device_id;
	const char* name;
};


const DeviceDescription* lookup_device_id(uint32_t device_id);

#endif /* __DEVICE_REGISTRY_H */
