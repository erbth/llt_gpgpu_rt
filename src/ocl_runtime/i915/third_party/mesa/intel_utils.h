/*
 * This file carries portions of MESA's code as noted.
 */
#ifndef __INTEL_UTILS_H
#define __INTEL_UTILS_H

#include <errno.h>
#include <sys/ioctl.h>

#include "../drm-uapi/i915_drm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* From src/intel/common/intel_gem.h */
/**
 * Call ioctl, restarting if it is interrupted
 */
static inline int
intel_ioctl(int fd, unsigned long request, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    return ret;
}

/* From src/intel/common/intel_gem.h */
/**
 * A wrapper around DRM_IOCTL_I915_QUERY
 *
 * Unfortunately, the error semantics of this ioctl are rather annoying so
 * it's better to have a common helper.
 */
static inline int
intel_i915_query_flags(int fd, uint64_t query_id, uint32_t flags,
                       void *buffer, int32_t *buffer_len)
{
   struct drm_i915_query_item item = {
      .query_id = query_id,
      .length = *buffer_len,
      .flags = flags,
      .data_ptr = (uintptr_t)buffer,
   };

   struct drm_i915_query args = {
      .num_items = 1,
      .flags = 0,
      .items_ptr = (uintptr_t)&item,
   };

   int ret = intel_ioctl(fd, DRM_IOCTL_I915_QUERY, &args);
   if (ret != 0)
      return -errno;
   else if (item.length < 0)
      return item.length;

   *buffer_len = item.length;
   return 0;
}

/* From src/intel/common/intel_gem.h */
static inline int
intel_i915_query(int fd, uint64_t query_id, void *buffer,
                 int32_t *buffer_len)
{
   return intel_i915_query_flags(fd, query_id, 0, buffer, buffer_len);
}

/* From src/intel/common/intel_gem.h */
/**
 * Query for the given data, allocating as needed
 *
 * The caller is responsible for freeing the returned pointer.
 */
static inline void *
intel_i915_query_alloc(int fd, uint64_t query_id, int32_t *query_length)
{
   if (query_length)
      *query_length = 0;

   int32_t length = 0;
   int ret = intel_i915_query(fd, query_id, NULL, &length);
   if (ret < 0)
      return NULL;

   void *data = calloc(1, length);
   assert(data != NULL); /* This shouldn't happen in practice */
   if (data == NULL)
      return NULL;

   ret = intel_i915_query(fd, query_id, data, &length);
   assert(ret == 0); /* We should have caught the error above */
   if (ret < 0) {
      free(data);
      return NULL;
   }

   if (query_length)
      *query_length = length;

   return data;
}

#ifdef __cplusplus
}
#endif

#endif /* __INTEL_UTILS_H */
