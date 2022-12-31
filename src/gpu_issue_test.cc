#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include "drm_interface.h"

using namespace std;


#define MI_BATCH_BUFFER_END (0xA << 23)


void main_exec_save()
{
	DRMInterface drm("/dev/dri/card0");

	{
		auto [major, minor, patch] = drm.get_driver_version();
		printf("DRM driver: %s, version: %d.%d.%d\n",
				drm.get_driver_name().c_str(), major, minor, patch);
	}

	printf("Chipset id: 0x%04x (%s), revision: 0x%04x\n",
			(int) drm.get_device_id(), drm.get_device_name(),
			(int) drm.get_device_revision());

	auto buf = drm.create_buffer(8192);
	auto addr = buf.map();

	const uint32_t bbe = MI_BATCH_BUFFER_END;
	memcpy(addr, &bbe, sizeof(bbe));

	buf.unmap();
}

int main(int argc, char** argv)
{
	try
	{
		main_exec_save();
	}
	catch (const exception& e)
	{
		fprintf(stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
