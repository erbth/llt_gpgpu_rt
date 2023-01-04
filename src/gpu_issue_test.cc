#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>

#include "drm_interface.h"
#include "compiler/igc_interface.h"
#include "kernel.h"

using namespace std;


#define MI_BATCH_BUFFER_END (0xA << 23)


const char* kernel_src = R"KERNEL(
__kernel void test_kernel(uint val, __global uint* restrict dst, uint size)
{
	if (get_global_id(0) < size)
		dst[get_global_id(0)] = val;
}
)KERNEL";


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

	/* Print short hw description */
	auto hw_info = drm.get_hw_info();
	auto hw_config = drm.get_hw_config();
	printf("Configuration: %d/%d/%d (%dx%dx%d)\n",
			(int) hw_info.gtSystemInfo.SliceCount,
			(int) hw_info.gtSystemInfo.SubSliceCount,
			(int) hw_info.gtSystemInfo.EUCount,
			(int) ((hw_config >> 32) & 0xffff),
			(int) ((hw_config >> 16) & 0xffff),
			(int) (hw_config & 0xffff));


	/* Compile a simple kernel */
	IGCInterface igc(hw_info);
	auto kernel_bin = igc.build(kernel_src, "-cl-std=CL1.2");
	auto build_log = igc.get_build_log();

	if (!kernel_bin)
	{
		printf("Failed to compile kernel:\n%s", build_log.c_str());
		throw runtime_error("Failed to compile kernel");
	}

	if (build_log.size() > 0)
		printf("Build log:\n%s\n", build_log.c_str());

	auto k = Kernel::read_kernel(kernel_bin->get_bin(), kernel_bin->bin_size, "test_kernel");




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
