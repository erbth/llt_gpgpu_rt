/** Low-level utilities for talking to the DRM device and other things */
#ifndef __I915_UTILS_H
#define __I915_UTILS_H

#include <vector>
#include <utility>
#include <xf86drm.h>
#include "third_party/drm-uapi/i915_drm.h"

namespace OCL {

long get_page_size();
drm_version_t get_drm_version(int fd, char* driver_name, size_t driver_name_size);

uint32_t gem_create(int fd, uint64_t* size);

/* NOTE: ptr, size must be aligned to the system's page size */
uint32_t gem_userptr(int fd, void* ptr, uint64_t size, bool probe);

void gem_close(int fd, uint32_t handle);
int gem_mmap_gtt_version(int fd);
int i915_getparam(int fd, int32_t param);
bool gem_supports_wc_mmap(int fd);
uint32_t gem_context_create(int fd);
void gem_context_destroy(int fd, uint32_t id);
void gem_context_set_vm(int fd, uint32_t ctx_id, uint32_t vm_id);

uint32_t gem_vm_create(int fd);
void gem_vm_destroy(int fd, uint32_t id);

void gem_execbuffer2(int fd, uint32_t ctx_id,
		std::vector<std::pair<uint32_t, void*>>& bos, size_t batch_len);

int64_t gem_wait(int fd, uint32_t bo, int64_t timeout_ns);

}

#endif /* __I915_UTILS_H */
