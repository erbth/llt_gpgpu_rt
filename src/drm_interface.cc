/** See also:
 *    * igt-gpu-tools*, especially tests/i915/gem_exec_nop.c
 *    * https://bwidawsk.net/blog/2013/8/i915-command-submission-via-gem_exec_nop/
 *    * https://bwidawsk.net/blog/2013/1/i915-hardware-contexts-and-some-bits-about-batchbuffers/
 *    * https://blog.ffwll.ch/2013/01/i915gem-crashcourse-overview.html
 *    * Mesa's codebase
 *
 * References:
 *   * igt-gpu-tools: https://gitlab.freedesktop.org/drm/igt-gpu-tools
 */
#include <cstring>
#include <cerrno>
#include <system_error>
#include <stdexcept>
#include "drm_interface.h"

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
}

using namespace std;


#define ARRAYSIZE(X) (sizeof(X) / sizeof(*X))


/* Helper functions */
/* adapted from igt-gpu-tools lib/i915/gem_create.c */
uint32_t gem_create(int fd, uint64_t* size)
{
	struct drm_i915_gem_create create = { 0 };
	create.size = *size;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create))
		throw runtime_error("DRM_IOCTL_I915_GEM_CREATE failed");

	*size = create.size;

	return create.handle;
}

/* adapted from igt-gpu-tools lib/ioctl_wrappers.c */
void gem_close(int fd, uint32_t handle)
{
	struct drm_gem_close close = { 0 };
	close.handle = handle;

	if (drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close))
		throw runtime_error("DRM_IOCTL_GEM_CLOSE failed");
}

/* adapted from igt-gpu-tools lib/i915/gem_mman.c */
int gem_mmap_gtt_version(int fd)
{
	struct drm_i915_getparam gp = { 0 };
	int gtt_version = -1;

	gp.param = I915_PARAM_MMAP_GTT_VERSION;
	gp.value = &gtt_version;
	if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
		throw runtime_error("DRM_IOCTL_I915_GETPARAM(MMAP_GTT_VERSION) failed");

	return gtt_version;
}

int i915_getparam(int fd, int32_t param)
{
	struct drm_i915_getparam gp = { 0 };
	int value = -1;

	gp.param = param;
	gp.value = &value;
	if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
		throw runtime_error("DRM_IOCTL_I915_GETPARAM failed");

	return value;
}

/* adapted from igt-gpu-tools lib/i915/gem_mman.c 
 * specifically: gem_mmap__has_wc */
bool gem_supports_wc_mmap(int fd)
{
	struct drm_i915_getparam gp = { 0 };
	int mmap_version;

	gp.param = I915_PARAM_MMAP_VERSION;
	gp.value = &mmap_version;
	if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
		throw runtime_error("DRM_IOCTL_I915_GETPARAM(MMAP_VERSION) failed");

	/* "Do we have the mmap_ioctl with DOMAIN_WC?" [from gem_mmap__has_wc] */
	if (mmap_version < 1)
		return false;

	if (gem_mmap_gtt_version(fd) < 2)
		return false;

	/* Test if wc-mmaps work on this device */
	uint64_t size = 4096;
	struct drm_i915_gem_mmap arg = { 0 };

	arg.handle = gem_create(fd, &size);
	arg.offset = 0;
	arg.size = 4096;
	arg.flags = I915_MMAP_WC;

	bool has_wc = drmIoctl(fd, DRM_IOCTL_I915_GEM_MMAP, &arg) == 0;
	if (has_wc && arg.addr_ptr)
	{
		if (munmap((void*)(uintptr_t) arg.addr_ptr, arg.size) < 0)
		{
			gem_close(fd, arg.handle);
			throw system_error(errno, generic_category(), "munmap");
		}
	}

	gem_close(fd, arg.handle);

	return has_wc;
}


/******************************** DRMInterface ********************************/
DRMInterface::DRMInterface(const char* device)
	: device_path(device)
{
	/* Ensure that the page size is 4kib */
	if (sysconf(_SC_PAGESIZE) != 4096)
		throw runtime_error("The system's page size is not 4096.");

	/* Try to open DRM device */
	fd = open(device_path.c_str(), O_RDWR);
	if (fd < 0)
		throw system_error(errno, generic_category(), "Failed to open DRM device");

	/* Query driver version */
	memset(driver_name, 0, ARRAYSIZE(driver_name));

	memset(&version, 0, sizeof(version));
	version.name_len = ARRAYSIZE(driver_name) - 1;
	version.name = driver_name;

	if (drmIoctl(fd, DRM_IOCTL_VERSION, &version))
	{
		close(fd);
		throw runtime_error("DRM_IOCTL_VERSION failed");
	}

	driver_name[ARRAYSIZE(driver_name) - 1] = '\0';

	if (strcmp(driver_name, "i915") != 0)
	{
		close(fd);
		throw runtime_error(string("Unsupported DRM driver:") + driver_name);
	}

	/* Query chipset id and revision */
	device_id = i915_getparam(fd, I915_PARAM_CHIPSET_ID);
	device_revision = i915_getparam(fd, I915_PARAM_REVISION);

	dev_desc = lookup_device_id(device_id);
	if (!dev_desc)
	{
		close(fd);
		throw runtime_error("Unknown device id");
	}

	/* We only support cpu-cache coherent write-combining mmap topologies by
	 * now (i.e. newer ingrated graphics). */
	if (!gem_supports_wc_mmap(fd))
	{
		close(fd);
		throw runtime_error("Coherent wc mmap is not supported by GPU");
	}

	/* Check if we have execbuf2 */
	if (i915_getparam(fd, I915_PARAM_HAS_EXECBUF2) != 1)
	{
		close(fd);
		throw runtime_error("Device does not support EXECBUF2");
	}
}

DRMInterface::~DRMInterface()
{
	close(fd);
}


string DRMInterface::get_driver_name() const
{
	return string(driver_name);
}

tuple<int, int, int> DRMInterface::get_driver_version() const
{
	return {version.version_major, version.version_minor,
		version.version_patchlevel};
}

int DRMInterface::get_fd() const
{
	return fd;
}

drm_magic_t DRMInterface::get_magic()
{
	drm_magic_t magic;

	if (drmGetMagic(fd, &magic) != 0)
		throw runtime_error("Failed to get magic");

	return magic;
}

int DRMInterface::get_device_id() const
{
	return device_id;
}

int DRMInterface::get_device_revision() const
{
	return device_revision;
}

const char* DRMInterface::get_device_name() const
{
	return dev_desc->name;
}


DRMBuffer DRMInterface::create_buffer(uint64_t size)
{
	auto handle = gem_create(fd, &size);

	return DRMBuffer(*this, handle, size);
}


/********************************* DRMBuffer **********************************/
DRMBuffer::DRMBuffer(DRMInterface& drm, uint32_t handle, uint64_t size)
	: drm(drm), handle(handle), size(size)
{
	fd = drm.get_fd();
}

DRMBuffer::~DRMBuffer()
{
	/* NOTE: may call terminate if munmap failes (but then we have different
	 * problems anyway... */
	if (mapped)
		unmap();

	gem_close(fd, handle);
}

void* DRMBuffer::map()
{
	if (mapped)
		throw logic_error("Mapped already");

	struct drm_i915_gem_mmap arg = { 0 };
	arg.handle = handle;
	arg.offset = 0;
	arg.size = size;
	arg.flags = I915_MMAP_WC;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_MMAP, &arg))
		throw runtime_error("DRM_IOCTL_I915_GEM_MMAP failed");

	map_addr = (void*)(uintptr_t) arg.addr_ptr;
	map_size = arg.size;
	mapped = true;

	return map_addr;
}

void DRMBuffer::unmap()
{
	if (!mapped)
		throw logic_error("Not mapped");

	if (munmap(map_addr, map_size) < 0)
		throw system_error(errno, generic_category(), "munmap");

	mapped = false;
	map_addr = nullptr;
	map_size = 0;
}
