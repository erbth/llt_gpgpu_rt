/** Heavily inspired by OpenCL */
#ifndef __DRM_INTERFACE_H
#define __DRM_INTERFACE_H

#include <cstdint>
#include <string>
#include <tuple>

extern "C" {
#include <xf86drm.h>
#include <libdrm/i915_drm.h>
#include <libdrm/intel_bufmgr.h>
}

#include "device_registry.h"
#include "kernel.h"


/* Yep, this name is taken from OpenCL */
struct NDRange final
{
	const uint32_t x;
	const uint32_t y;
	const uint32_t z;

	NDRange(uint32_t x, uint32_t y = 1, uint32_t z = 1);
};

class DRMBuffer;

class DRMInterface
{
protected:
	std::string device_path;
	drm_version_t version;
	char driver_name[32];

	int fd = -1;
	uint32_t ctx_id = 0;

	/* Information about the device */
	int device_id;
	int device_revision;

	/* Assuming a homogoenous architecture (which is not true for all devices,
	 * on some one EU might be reserved) */
	int slice_cnt = 0;
	int subslice_cnt = 0;
	int eu_cnt = 0;

	DeviceDescription dev_desc{};

public:
	DRMInterface(const char* device);

	DRMInterface(const DRMInterface&) = delete;
	DRMInterface(DRMInterface&&) = delete;
	DRMInterface& operator=(const DRMInterface&) = delete;
	DRMInterface& operator=(DRMInterface&&) = delete;

	virtual ~DRMInterface();

	std::string get_driver_name() const;
	std::tuple<int, int, int> get_driver_version() const;
	int get_fd() const;

	drm_magic_t get_magic();

	int get_device_id() const;
	int get_device_revision() const;
	uint64_t get_hw_config() const;
	const char* get_device_name() const;
	NEO::HardwareInfo get_hw_info() const;


	/* Size will be rounded up to 4k pages */
	DRMBuffer create_buffer(uint64_t size);

	void exec_kernel(
			const Kernel& kernel,
			const NDRange& global_size,
			const NDRange& local_size);
};

/* Buffers created by a DRMInterface are only valid as long as the DRMInterface
 * exists. Using a buffer after the creating DRMInterface has been destroyed
 * results in (usually bad if fds are reused) undefined behavior. */
class DRMBuffer
{
protected:
	DRMInterface& drm;

	int fd;
	const uint32_t handle;
	const uint64_t size;

	bool mapped = false;
	void* map_addr = nullptr;
	uint64_t map_size = 0;

public:
	DRMBuffer(DRMInterface& drm, uint32_t handle, uint64_t size);

	DRMBuffer(const DRMBuffer&) = delete;
	DRMBuffer(DRMBuffer&&) = delete;
	DRMBuffer& operator=(const DRMBuffer&) = delete;
	DRMBuffer& operator=(DRMBuffer&&) = delete;

	virtual ~DRMBuffer();

	uint64_t get_size() const;
	uint32_t get_handle() const;

	void* map();
	void unmap();
};

#endif /* __DRM_INTERFACE_H */
