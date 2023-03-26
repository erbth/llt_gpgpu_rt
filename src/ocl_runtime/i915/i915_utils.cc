#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <new>
#include "i915_utils.h"

extern "C" {
#include <unistd.h>
#include <sys/mman.h>
}

using namespace std;

namespace OCL {

long get_page_size()
{
	auto page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 0)
		throw system_error(errno, generic_category(), "Failed to query the page size");

	return page_size;
}

drm_version_t get_drm_version(int fd, char* driver_name, size_t driver_name_size)
{
	drm_version_t version{};

	version.name_len = driver_name_size - 1;
	version.name = driver_name;

	if (drmIoctl(fd, DRM_IOCTL_VERSION, &version))
		throw runtime_error("DRM_IOCTL_VERSION failed");

	driver_name[driver_name_size - 1] = '\0';

	return version;
}

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

uint32_t gem_userptr(int fd, void* ptr, uint64_t size, bool probe)
{
	struct drm_i915_gem_userptr cmd = { 0 };
	cmd.user_ptr = (uintptr_t) ptr;
	cmd.user_size = size;
	cmd.flags = 0;

	if (probe)
		cmd.flags |= I915_USERPTR_PROBE;

	auto ret_code = drmIoctl(fd, DRM_IOCTL_I915_GEM_USERPTR, &cmd);
	if (ret_code)
	{
		if (ret_code == -EFAULT)
		{
			throw runtime_error("DRM_IOCTL_I915_GEM_USERPTR: probe failed - "
					"perhaps the memory range is invalid");
		}

		throw runtime_error("DRM_IOCTL_I915_GEM_USERPTR failed");
	}

	return cmd.handle;
}

void gem_open(int fd, uint32_t name, uint32_t& handle, uint64_t& size)
{
	struct drm_gem_open cmd = { 0 };
	cmd.name = name;

	if (drmIoctl(fd, DRM_IOCTL_GEM_OPEN, &cmd))
		throw system_error(errno, generic_category(), "DRM_IOCTL_GEM_OPEN failed");

	handle = cmd.handle;
	size = cmd.size;
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
		throw system_error(errno, generic_category(), "DRM_IOCTL_I915_GETPARAM failed");

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

uint32_t gem_context_create(int fd)
{
	struct drm_i915_gem_context_create arg = { 0 };
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &arg))
		throw runtime_error("DRM_IOCTL_I915_GEM_CONTEXT_CREATE failed");

	return arg.ctx_id;
}

void gem_context_destroy(int fd, uint32_t id)
{
	struct drm_i915_gem_context_destroy arg = { 0 };
	arg.ctx_id = id;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &arg))
		throw runtime_error("DRM_IOCTL_I915_GEM_CONTEXT_DESTROY failed");
}

void gem_context_set_vm(int fd, uint32_t ctx_id, uint32_t vm_id)
{
	struct drm_i915_gem_context_param cmd = { 0 };
	cmd.ctx_id = ctx_id;
	cmd.param = I915_CONTEXT_PARAM_VM;
	cmd.value = vm_id;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &cmd))
		throw system_error(errno, generic_category(), "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM");
}

uint32_t gem_vm_create(int fd)
{
	struct drm_i915_gem_vm_control cmd = { 0 };

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_VM_CREATE, &cmd))
		throw system_error(errno, generic_category(), "DRM_IOCTL_I915_GEM_VM_CREATE");

	return cmd.vm_id;
}

void gem_vm_destroy(int fd, uint32_t id)
{
	struct drm_i915_gem_vm_control cmd = { 0 };
	cmd.vm_id = id;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_VM_DESTROY, &cmd))
		throw system_error(errno, generic_category(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
}

void gem_execbuffer2(int fd, uint32_t ctx_id,
		vector<tuple<uint32_t, void*, vector<struct drm_i915_gem_relocation_entry>>>& bos,
		size_t batch_len)
{
	auto objs = new struct drm_i915_gem_exec_object2[bos.size()];
	try
	{
		bool no_reloc = true;
		memset(objs, 0, sizeof(*objs) * bos.size());

		for (size_t i = 0; i < bos.size(); i++)
		{
			auto obj = objs + i;
			obj->handle = get<0>(bos[i]);
			obj->offset = (uintptr_t) get<1>(bos[i]);
			obj->flags = EXEC_OBJECT_SUPPORTS_48B_ADDRESS;

			auto& relocs = get<2>(bos[i]);
			if (relocs.size() > 0)
			{
				no_reloc = false;
				obj->relocation_count = relocs.size();
				obj->relocs_ptr = (uintptr_t) relocs.data();
			}
			else
			{
				obj->flags |= EXEC_OBJECT_PINNED;
			}
		}

		struct drm_i915_gem_execbuffer2 cmd = { 0 };
		cmd.buffers_ptr = (uintptr_t) objs;
		cmd.buffer_count = bos.size();
		cmd.batch_start_offset = 0;
		cmd.batch_len = batch_len;
		cmd.flags = I915_EXEC_RENDER;

		if (no_reloc)
			cmd.flags |= I915_EXEC_NO_RELOC;

		i915_execbuffer2_set_context_id(cmd, ctx_id);

		if (drmIoctl(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &cmd))
			throw system_error(errno, generic_category(), "DRM_IOCTL_I915_GEM_EXECBUFFER2 failed");

		delete[] objs;
	}
	catch (...)
	{
		delete[] objs;
	}
}

int64_t gem_wait(int fd, uint32_t bo, int64_t timeout_ns)
{
	struct drm_i915_gem_wait cmd = { 0 };
	cmd.bo_handle = bo;
	cmd.timeout_ns = timeout_ns;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_WAIT, &cmd))
		throw system_error(errno, generic_category(), "DRM_IOCTL_I915_GEM_WAIT failed");

	/* Remaining time */
	return cmd.timeout_ns;
}

}
