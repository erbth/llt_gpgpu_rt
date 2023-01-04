#include <cstdio>
#include <stdexcept>
#include <functional>
#include "device_registry.h"

using namespace std;


template <typename  T>
NEO::HardwareInfo generate_hw_info(uint64_t config, uint32_t device_id, uint32_t device_revision)
{
	NEO::HardwareInfo hw_info(T::hwInfo);
	hw_info.platform.usDeviceID = device_id;
	hw_info.platform.usRevId = device_revision;

	NEO::setHwInfoValuesFromConfig(config, hw_info);
	T::setupHardwareInfo(&hw_info, true);

	return hw_info;
}

struct DeviceRecord
{
	uint32_t device_id;
	const char* name;
	function<NEO::HardwareInfo(uint64_t, uint32_t, uint32_t)> gen_hw_info;
};

static DeviceRecord dev_table[] = {

#define DEVICE(I,C) { I, "<unnamed>", generate_hw_info<NEO::C> },
#define NAMEDDEVICE(I,C,N) { I, N, generate_hw_info<NEO::C> },
#include "third_party/devices.inl"
#undef NAMEDDEVICE
#undef DEVICE

};


DeviceDescription::DeviceDescription(uint32_t device_id, const char* name,
		const NEO::HardwareInfo& hw_info)
	:
		device_id(device_id), name(name), hw_info(hw_info)
{
}


DeviceDescription lookup_device_id(uint32_t device_id, uint32_t device_revision,
		uint64_t hw_config)
{
	for (size_t i = 0; i < sizeof(dev_table) / sizeof(*dev_table); i++)
	{
		if (dev_table[i].device_id == device_id)
		{
			auto& e = dev_table[i];
			return DeviceDescription(
					e.device_id, e.name,
					e.gen_hw_info(hw_config, device_id, device_revision));
		}
	}

	char buf[16];
	snprintf(buf, 16, "0x%04x", (int) device_id);
	buf[sizeof(buf) / sizeof(*buf) - 1] = '\0';
	throw runtime_error(string("Unsupported device id: ") + buf);
}
