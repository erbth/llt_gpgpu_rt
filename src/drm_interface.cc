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
#include <memory>
#include <vector>
#include <system_error>
#include <stdexcept>
#include "utility.h"
#include "drm_interface.h"

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
}

#include "hw_cmds.h"
#define PAGE_SIZE (4096ULL)

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


/******************************** DRMInterface ********************************/
NDRange::NDRange(uint32_t x, uint32_t y, uint32_t z)
	: x(x), y(y), z(z)
{
}

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

	try
	{
		/* Query driver version */
		memset(driver_name, 0, ARRAYSIZE(driver_name));

		memset(&version, 0, sizeof(version));
		version.name_len = ARRAYSIZE(driver_name) - 1;
		version.name = driver_name;

		if (drmIoctl(fd, DRM_IOCTL_VERSION, &version))
			throw runtime_error("DRM_IOCTL_VERSION failed");

		driver_name[ARRAYSIZE(driver_name) - 1] = '\0';

		if (strcmp(driver_name, "i915") != 0)
			throw runtime_error(string("Unsupported DRM driver:") + driver_name);

		/* Query chipset id and revision */
		device_id = i915_getparam(fd, I915_PARAM_CHIPSET_ID);
		device_revision = i915_getparam(fd, I915_PARAM_REVISION);

		/* Query device GPGPU EU configuration */
		auto eu_total = i915_getparam(fd, I915_PARAM_EU_TOTAL);
		auto subslice_total = i915_getparam(fd, I915_PARAM_SUBSLICE_TOTAL);
		unsigned int slice_mask = i915_getparam(fd, I915_PARAM_SLICE_MASK);

		slice_cnt = 0;
		while (slice_mask > 0)
		{
			slice_cnt++;

			do {
				slice_mask >>= 1;
			}
			while (slice_mask > 0 && (slice_mask & 0x1) == 0);
		}

		/* Round up to account for reserved EUs */
		subslice_cnt = (subslice_total + slice_cnt-1) / slice_cnt;
		eu_cnt = (eu_total + subslice_total-1) / subslice_total;


		dev_desc = lookup_device_id(0x3185, device_revision, get_hw_config());

		/* Only support Gen9 for now */
		if (dev_desc.hw_info.platform.eRenderCoreFamily != IGFX_GEN9_CORE)
			throw runtime_error("Currently only Gen9 devices are supported");

		/* We only support cpu-cache coherent write-combining mmap topologies by
		 * now (i.e. newer ingrated graphics). */
		if (!gem_supports_wc_mmap(fd))
			throw runtime_error("Coherent wc mmap is not supported by GPU");

		/* Check if we have execbuf2 */
		if (i915_getparam(fd, I915_PARAM_HAS_EXECBUF2) != 1)
			throw runtime_error("Device does not support EXECBUF2");

		/* Check if other required capabilities are supported */
		if (i915_getparam(fd, I915_PARAM_HAS_EXEC_NO_RELOC) != 1)
			throw runtime_error("Devices does not suport EXEC_NO_RELOC");

		if (i915_getparam(fd, I915_PARAM_HAS_EXEC_HANDLE_LUT) != 1)
			throw runtime_error("Devices does not suport EXEC_HANDLE_LUT");

		/* Create context */
		ctx_id = gem_context_create(fd);
	}
	catch (...)
	{
		close(fd);
		throw;
	}
}

DRMInterface::~DRMInterface()
{
	gem_context_destroy(fd, ctx_id);
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

uint64_t DRMInterface::get_hw_config() const
{
	return ((uint64_t) slice_cnt << 32) |
		((uint64_t) subslice_cnt << 16) |
		((uint64_t) eu_cnt);
}

const char* DRMInterface::get_device_name() const
{
	return dev_desc.name;
}

NEO::HardwareInfo DRMInterface::get_hw_info() const
{
	return dev_desc.hw_info;
}


DRMBuffer DRMInterface::create_buffer(uint64_t size)
{
	size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1ULL);
	auto handle = gem_create(fd, &size);

	return DRMBuffer(*this, handle, size);
}


void DRMInterface::exec_kernel(
		const Kernel& kernel,
		const NDRange& global_size,
		const NDRange& local_size)
{
	printf("\nExecuting kernel `%s':\n", kernel.name.c_str());

	/* Require patch tokens */
	if (
			!kernel.params.media_interface_descriptor_load.present ||
			!kernel.params.interface_descriptor_data.present ||
			!kernel.params.thread_payload.present ||
			!kernel.params.execution_environment.present)
	{
		throw invalid_argument("Mandatory patch tokens missing");
	}

	if (
			kernel.params.kernel_attributes_info.present &&
			kernel.params.kernel_attributes_info.attributes.size() > 0)
	{
		throw invalid_argument("Kernel attributes are not supported yet.");
	}

	/* Setup interface descriptor */
	auto& int_desc_data = kernel.params.interface_descriptor_data;
	auto& media_int_load = kernel.params.media_interface_descriptor_load;

	printf("interface_descriptor: offset: 0x%x, sampler_state_offset: 0x%x, "
			"kernel_offset: 0x%x, binding_table_offset: 0x%x\n",
			(unsigned) int_desc_data.offset,
			(unsigned) int_desc_data.sampler_state_offset,
			(unsigned) int_desc_data.kernel_offset,
			(unsigned) int_desc_data.binding_table_offset);

	printf("media_interface_descriptor: data_offset: 0x%x\n",
			(unsigned) media_int_load.data_offset);

	/* Currently, only the interface descriptor is supported in the dynamic
	 * state heap */
	if (kernel.dynamic_state_heap->size != 32)
		throw invalid_argument("A kernel with dynamic heap size != 32 is not supported yet");

	static_assert(sizeof(HWCmds::Gen9::InterfaceDescriptorData) == 32);
	auto const src_int_desc = kernel.dynamic_state_heap->ptr() + media_int_load.data_offset;
	if (media_int_load.data_offset != 0)
		throw invalid_argument("The interface descriptor lays outside of the dynamic state heap");

	HWCmds::Gen9::InterfaceDescriptorDataAccessor int_desc;
	int_desc.common.data = *((HWCmds::Gen9::InterfaceDescriptorData*) src_int_desc);

	if (int_desc.get_kernel_start_pointer() != 0)
		throw invalid_argument("Currently only kernels with initial IP = 0x0 are supported.");

	/* Compare values from patch list and dynamic state heap initialization */
	if (
			media_int_load.data_offset != int_desc_data.offset ||
			int_desc.common.sampler_state_pointer.get() != int_desc_data.sampler_state_offset ||
			int_desc.get_kernel_start_pointer() != int_desc_data.kernel_offset ||
			int_desc.common.binding_table_pointer.get() * 32 != int_desc_data.binding_table_offset)
	{
		throw invalid_argument("Mismatch between interface descriptor data "
				"patch token, media interface load patch token, and interface "
				"descriptor supplied in dynamic state heap initialization value.");
	}

	/* Override/assert some values */
	int_desc.set_kernel_start_pointer(0x1000000);
	int_desc.common.thread_priority.set(0);

	if (int_desc.common.constant_indirect_urb_entry_read_length.get() != 0)
		throw invalid_argument("per thread URB read length must be 0");

	if (kernel.params.execution_environment.has_barriers)
		int_desc.common.barrier_enable.set(1);

	uint32_t slm_size = 0;
	uint32_t slm_offset = 0;
	if (kernel.params.allocate_local_surface)
	{
		slm_size = kernel.params.allocate_local_surface->total_inline_local_memory_size;
		slm_offset = kernel.params.allocate_local_surface->offset;

		if (slm_offset != 0)
			throw invalid_argument("SLM offset != 0: " + to_hex_string(slm_offset));

		if (slm_size > 64 * 1024)
			throw invalid_argument("Requested SLM too large: " + to_string(slm_size));

		int_desc.set_shared_local_memory_size_bytes(slm_size);

		printf("SLM size: %d, SLM offset: %d\n", (int) slm_size, (int) slm_offset);
	}

	int_desc.common.global_barrier_enable.set(0);

	/* Compute thread group size */
	auto simd_size = kernel.params.execution_environment.largest_compiled_simd_size;
	auto n_x_threads = (local_size.x + simd_size - 1) / simd_size;
	auto n_threads = n_x_threads * local_size.y * local_size.z;

	if (n_threads > 1024)
		throw invalid_argument("Maximum group size is 1024");

	printf("SIMD%d, %dx%dx%d threads\n", (int) simd_size,
			(int) n_x_threads, (int) local_size.y, (int) local_size.z);

	int_desc.common.number_of_threads_in_gpgpu_thread_group.set(n_threads);

	/* Thread payload / CURBE */
	if (int_desc.common.constant_indirect_urb_entry_read_length.get() != 0)
		throw runtime_error("per-thread urb data present");

	if (int_desc.common.constant_urb_entry_read_offset.get() != 0)
		throw runtime_error("per-thread urb data offset != 0");

	printf("curbe: %d\n", (int) int_desc.common.cross_thread_constant_data_read_length.get());












	/* Load instruction buffer into GPU memory */
	auto instruction_buffer = create_buffer(kernel.kernel_heap->unpadded_size);

	{
		auto base = instruction_buffer.map();
		memcpy(base, kernel.kernel_heap->ptr(), kernel.kernel_heap->unpadded_size);
		instruction_buffer.unmap();
	}

	/* Setup heaps */
	/* Dynamic state */
	size_t dynamic_state_size = kernel.dynamic_state_heap->size;
	auto dynamic_state_heap = create_buffer(dynamic_state_size);

	{
		auto base = dynamic_state_heap.map();
		memcpy(base, &int_desc, sizeof(int_desc));

		printf("\nkernel dynamic size: %d\n", (int) kernel.dynamic_state_heap->unpadded_size);
		for (unsigned i = 0; i < dynamic_state_size; i += 4)
		{
			if (i % 16 == 0)
				printf("0x%04x:", i);

			printf(" %08x", (int) *((uint32_t*) ((char*) base + i)));

			if (i % 16 == 12)
				printf("\n");
		}

		dynamic_state_heap.unmap();
	}




	printf("\nkernel surface size: %d\n", (int) kernel.surface_state_heap->unpadded_size);

	for (unsigned i = 0; i < kernel.surface_state_heap->unpadded_size; i += 4)
	{
		if (i % 16 == 0)
			printf("0x%04x:", i);

		printf(" %08x", (int) *((uint32_t*) (kernel.surface_state_heap->ptr() + i)));

		if (i % 16 == 12)
			printf("\n");
	}

	printf("\n");

	throw runtime_error("abort");

	/* Create a list of commands */
	vector<unique_ptr<HWCmds::Command>> cmds;

	/* PIPE_CONTROL to wait for ongoing work to finish and flush caches */
	auto p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	p->flush_caches = true;
	cmds.push_back(move(p));

	p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	p->generic_media_state_clear = true;
	p->invalidate_caches = true;
	cmds.push_back(move(p));

	/* PIPELINE_SELECT to switch to 3D mode */
	auto ps = make_unique<HWCmds::Gen9::PipelineSelect>();
	ps->pipeline_selection = HWCmds::Gen9::PipelineSelect::PIPELINE_3D;
	cmds.push_back(move(ps));

	/* Clear COLOR_CALC_STATE_VALID */
	auto _3dstateccstatepointers = make_unique<HWCmds::Gen9::_3DStateCCStatePointers>();
	_3dstateccstatepointers->color_calc_state_pointer_valid = false;
	cmds.push_back(move(_3dstateccstatepointers));

	/* (No need to disable resource streamer as it is not enabled for this batch
	 * buffer. i915.ko does not support it anymore, anyway.) */

	/* Disable hardware binding tables */
	auto _3dstatebindingtablepoolalloc = make_unique<HWCmds::Gen9::_3DStateBindingTablePoolAlloc>();
	cmds.push_back(move(_3dstatebindingtablepoolalloc));

	/* Another PIPE_CONTROL to wait for ongoing work to finish and flush caches */
	p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	p->flush_caches = true;
	cmds.push_back(move(p));

	p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	p->generic_media_state_clear = true;
	p->invalidate_caches = true;
	cmds.push_back(move(p));

	/* PIPELINE_SELECT to switch to GPGPU mode */
	ps = make_unique<HWCmds::Gen9::PipelineSelect>();
	ps->force_media_awake = true;
	ps->dop_clock_enable = false;
	ps->pipeline_selection = HWCmds::Gen9::PipelineSelect::PIPELINE_GPGPU;
	cmds.push_back(move(ps));

	/* CS stall + flush caches */
	p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	p->flush_caches = true;
	cmds.push_back(move(p));


	/* Set state pointers */
	auto sba = make_unique<HWCmds::Gen9::StateBaseAddress>();

	// sba->instruction_base_address = instruction_buffer.get_pin_address();
	sba->instruction_buffer_size = instruction_buffer.get_size() / PAGE_SIZE;

	cmds.push_back(move(sba));

	/* MEDIA_VFE_STATE */
	p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	cmds.push_back(move(p));

	auto mvs = make_unique<HWCmds::Gen9::MediaVFEState>();
	cmds.push_back(move(mvs));

	/* MEDIA_INTERFACE_DESCRIPTOR_LOAD */
	cmds.push_back(make_unique<HWCmds::Gen9::MediaStateFlush>());

	auto midl = make_unique<HWCmds::Gen9::MediaInterfaceDescriptorLoad>();
	cmds.push_back(move(midl));

	/* MEDIA_CURBE_LOAD */
	auto mcl = make_unique<HWCmds::Gen9::MediaCurbeLoad>();
	cmds.push_back(move(mcl));


	/* GPGPU_WALKER */
	auto gw = make_unique<HWCmds::Gen9::GPGPUWalker>();
	cmds.push_back(move(gw));

	/* Gen9 PRM requires to send a MEDIA_STATE_FLUSH "with no options" after a
	 * GPGPU_WALKER command if neither SLM nor barriers are used */
	cmds.push_back(make_unique<HWCmds::Gen9::MediaStateFlush>());


	/* Flush pipeline */
	p = make_unique<HWCmds::Gen9::PipeControl>();
	p->cs_stall = true;
	p->flush_caches = true;
	cmds.push_back(move(p));


	/* MI_BATCH_BUFFER_END */
	cmds.push_back(make_unique<HWCmds::Gen9::MiBatchBufferEnd>());

	/* End with at least one MI_NOOP (see
	 * https://bwidawsk.net/blog/2013/8/i915-command-submission-via-gem_exec_nop/)
	 * */
	cmds.push_back(make_unique<HWCmds::Gen9::MiNoop>());


	/* Write commands to buffer */
	size_t constexpr cmd_buf_size = 4096;
	auto cmd_buf = create_buffer(4096);
	auto cmd_ptr = (char*) cmd_buf.map();

	/* NOTE: not checking the size in advance is somewhat dangerous... */
	auto written = HWCmds::write_cmds(cmd_ptr, cmds.cbegin(), cmds.cend());
	if (written > cmd_buf_size)
		throw runtime_error("Wrote too many commands to cmd buffer");

	fprintf(stderr, "Batch buffer size: %d\n", (int) written);

	cmd_buf.unmap();

	/* Execute buffer */
	struct drm_i915_gem_execbuffer2 execbuf = { 0 };
	struct drm_i915_gem_exec_object2 execobj = { 0 };

	execobj.handle = cmd_buf.get_handle();

	execbuf.buffers_ptr = (uintptr_t) &execobj;
	execbuf.buffer_count = 1;
	execbuf.batch_len = written;
	execbuf.flags = I915_EXEC_RENDER | I915_EXEC_HANDLE_LUT | I915_EXEC_NO_RELOC;
	i915_execbuffer2_set_context_id(execbuf, ctx_id);

	auto res = drmIoctl(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
	if (res != 0)
		throw system_error(errno, generic_category(), "DRM_IOCTL_I915_EXECBUFFER2 failed");
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

uint64_t DRMBuffer::get_size() const
{
	return size;
}

uint32_t DRMBuffer::get_handle() const
{
	return handle;
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
