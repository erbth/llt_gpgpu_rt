#include <stdexcept>
#include "device_registry.h"

extern "C" {
#include <igdgmm.h>
}

using namespace std;


/* List of supported device families
 * NOTE: When changing these (especially when adding older families), check that
 * the rest of the codebase supports the new families. */
#define SUPPORT_GEN9
#define SUPPORT_CFL
#define SUPPORT_GLK

/* HW info from intel-compute-runtime ("NEO") */
// #include "third_party/hwinfo/hw_info.h"

static DeviceDescription dev_descs[] = {

#define DEVICE(I,C) { I, "<unnamed>" },
#define NAMEDDEVICE(I,C,N) { I, N },
#include "third_party/devices.inl"
#undef NAMEDDEVICE
#undef DEVICE

};


const DeviceDescription* lookup_device_id(uint32_t device_id)
{
	for (size_t i = 0; i < sizeof(dev_descs) / sizeof(*dev_descs); i++)
	{
		if (dev_descs[i].device_id == device_id)
			return &dev_descs[i];
	}

	return nullptr;
}
