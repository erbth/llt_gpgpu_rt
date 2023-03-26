/** Heavily inspired by OpenCL */
#ifndef __I915_RUNTIME_IMPL_H
#define __I915_RUNTIME_IMPL_H

#include <string>
#include <memory>
#include <vector>
#include "i915_runtime.h"
#include "igc_progbin.h"
#include "i915_kernel_utils.h"

extern "C" {
#include <xf86drm.h>
#include "third_party/drm-uapi/i915_drm.h"
#include <libdrm/intel_bufmgr.h>
}

#include "third_party/mesa/intel_device_info.h"

namespace OCL
{

/* Prototypes */
class I915PreparedKernelImpl;
class I915RTEImpl;

class I915KernelImpl : public I915Kernel
{
	friend I915PreparedKernelImpl;

protected:
	const std::string name;
	const KernelParameters params;

	std::unique_ptr<Heap> kernel_heap;
	std::unique_ptr<Heap> dynamic_state_heap;
	std::unique_ptr<Heap> surface_state_heap;

	std::string build_log;

public:
	I915KernelImpl(
			const std::string& name,
			const KernelParameters& params,
			std::unique_ptr<Heap>&& kernel_heap,
			std::unique_ptr<Heap>&& dynamic_state_heap,
			std::unique_ptr<Heap>&& surface_state_heap,
			const std::string& build_log);

	I915KernelImpl(const I915KernelImpl&) = delete;
	I915KernelImpl& operator=(const I915KernelImpl&) = delete;

	~I915KernelImpl();

	std::string get_build_log() override;

	static std::shared_ptr<I915KernelImpl> read_kernel(
			const char* bin, size_t size, const std::string& name,
			const std::string& build_log);
};


class I915PreparedKernelImpl : public I915PreparedKernel
{
protected:
	I915RTEImpl& rte;
	std::shared_ptr<I915KernelImpl> kernel;

	std::vector<std::unique_ptr<KernelArg>> args;

public:
	I915PreparedKernelImpl(I915RTEImpl& rte, std::shared_ptr<I915KernelImpl> kernel);

	I915PreparedKernelImpl(const I915PreparedKernelImpl&) = delete;
	I915PreparedKernelImpl& operator=(const I915PreparedKernelImpl&) = delete;

	~I915PreparedKernelImpl();

	void add_argument(uint32_t) override;
	void add_argument(int32_t) override;
	void add_argument(uint64_t) override;
	void add_argument(int64_t) override;

	/* @param size is in bytes */
	void add_argument(void*, size_t) override;
	void add_argument_gem_name(uint32_t name) override;

	template<typename T, const char* C>
	void add_argument_int(T);

	void execute(NDRange global_size, NDRange local_size) override;
};

/* NOTE: Keep care that the RTE is not destructed while objects of this class
 * exist. */
class I915UserptrBo final
{
protected:
	I915RTEImpl& rte;

	bool allocated;
	void* _ptr;
	size_t _size;
	uint32_t _handle;

public:
	/* Allocate buffer. @param req_size will be increased s.t. it is a multiple
	 * of the page size and used as size. Hence size() will be >= req_size, but
	 * NOT necessarily == req_size. */
	I915UserptrBo(I915RTEImpl& rte, size_t req_size);

	/* Use allocated buffer */
	I915UserptrBo(I915RTEImpl& rte, void* ptr, size_t size);

	I915UserptrBo(const I915UserptrBo&) = delete;
	I915UserptrBo& operator=(const I915UserptrBo&) = delete;

	~I915UserptrBo();

	void* ptr() const;
	size_t size() const;
	uint32_t handle() const;
};

class I915RTEImpl final : public I915RTE
{
	friend I915PreparedKernelImpl;

protected:
	const std::string device_path;
	long page_size = 0;

	char driver_name[32];
	drm_version_t driver_version;

	int fd = -1;
	uint32_t ctx_id = 0;
	uint32_t vm_id = 0;

	/* Information about the device */
	struct intel_device_info dev_info{};

	int dev_id = 0;
	int dev_revision = 0;

	bool has_userptr_probe = false;

public:
	I915RTEImpl(const char* device);

	I915RTEImpl(const I915RTEImpl&) = delete;
	I915RTEImpl& operator=(const I915RTEImpl&) = delete;

	~I915RTEImpl();

	std::shared_ptr<Kernel> compile_kernel(const char* src, const char* name,
			const char* options) override;

	std::unique_ptr<PreparedKernel> prepare_kernel(std::shared_ptr<Kernel> kernel) override;

	size_t get_page_size() override;
	size_t align_size_to_page(size_t size);

	uint32_t gem_userptr(void* ptr, size_t size);
	void gem_open(uint32_t name, uint32_t& handle, uint64_t& size);
	void gem_close(uint32_t handle);

	virtual drm_magic_t get_drm_magic() override;
};

}

#endif /* __I915_RUNTIME_IMPL_H */
