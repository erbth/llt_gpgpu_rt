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
#include <algorithm>
#include "utils.h"
#include "i915_kernel_utils.h"
#include "i915_runtime_impl.h"
#include "gen9_hw_int.h"

using namespace std;


namespace OCL {

KernelArg::~KernelArg()
{
}

KernelArgPtr::KernelArgPtr(size_t page_size, void* _ptr, size_t _size)
	: _ptr(_ptr), _size(_size)
{
	if ((uintptr_t) _ptr % page_size != 0 || _size % page_size != 0)
		throw invalid_argument("The pointer and size must be aligned to the page size");
}

KernelArgPtr::~KernelArgPtr()
{
}

void* KernelArgPtr::ptr()
{
	return _ptr;
}

size_t KernelArgPtr::size() const
{
	return _size;
}

KernelArgGEMName::KernelArgGEMName(I915RTEImpl& rte, uint32_t name)
	: rte(rte)
{
	rte.gem_open(name, _handle, _size);
}

KernelArgGEMName::~KernelArgGEMName()
{
	rte.gem_close(_handle);
}

uint32_t KernelArgGEMName::handle() const
{
	return _handle;
}

size_t KernelArgGEMName::size() const
{
	return _size;
}

I915RingCmd::~I915RingCmd()
{
}


void set_param(
		uint32_t offset, uint32_t data_size, uint64_t value,
		size_t& size, char* dst, size_t capacity)
{
	if (data_size > 8)
		throw runtime_error("data_size > 8");

	size_t req_size = offset + data_size;
	if (req_size > capacity)
		throw invalid_argument("Size written exceeds capacity");

	size = max(size, req_size);
	memcpy(dst + offset, &value, data_size);

	// printf("offset: %d, data_size: %d, value: 0x%llx\n",
	// 		(int) offset, (int) data_size, (long long) value);
}

template<typename T>
bool try_set_int_arg(const KernelArg* arg,
		uint32_t offset, uint32_t data_size,
		size_t& size, char* dst, size_t capacity)
{
	auto int_arg = dynamic_cast<const KernelArgInt<T>*>(arg);
	if (!int_arg)
		return false;

	if (data_size < sizeof(T))
		throw invalid_argument("data_size too small for type");

	set_param(
			offset,
			data_size,
			int_arg->value(),
			size, dst, capacity);

	return true;
}

size_t build_cross_thread_data(
		const KernelParameters& params,
		const NDRange& global_offset,
		const NDRange& local_size,
		const vector<unique_ptr<KernelArg>>& args,
		const char* surface_state_base, size_t surface_state_size,
		char* dst, size_t capacity,
		vector<tuple<uint32_t, uint64_t>>& relocs)
{
	size_t size = 0;

	for (auto& dpb : params.data_parameter_buffers)
	{
		// printf("\nargument_number: %d, source_offset: %d, location_index: %d, "
		// 		"location_index2: %d, is_emulation_argument: %d\n",
		// 		(int) dpb.argument_number, (int) dpb.source_offset,
		// 		(int) dpb.location_index, (int) dpb.location_index2,
		// 		(int) dpb.is_emulation_argument);

		switch (dpb.type)
		{
		case iOpenCL::DATA_PARAMETER_GLOBAL_WORK_OFFSET:
			{
				// printf("global work offset\n");

				uint32_t val = 0;
				switch (dpb.source_offset)
				{
				case 0:
					val = global_offset.x;
					break;

				case 4:
					val = global_offset.y;
					break;

				case 8:
					val = global_offset.z;
					break;

				default:
					throw invalid_argument("Invalid source_offset for DATA_PARAMETER_GLOBAL_WORK_OFFSET");
				}

				set_param(
						dpb.offset,
						dpb.data_size,
						val,
						size, dst, capacity);
			}
			break;

		case iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE:
			{
				// printf("local work size\n");

				uint32_t val = 0;
				switch (dpb.source_offset)
				{
				case 0:
					val = local_size.x;
					break;

				case 4:
					val = local_size.y;
					break;

				case 8:
					val = local_size.z;
					break;

				default:
					throw invalid_argument("Invalid source_offset for DATA_PARAMETER_LOCAL_WORK_SIZE");
				}

				set_param(
						dpb.offset,
						dpb.data_size,
						val,
						size, dst, capacity);
			}
			break;

		case iOpenCL::DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE:
			{
				// printf("enqueued local work size\n");

				uint32_t val = 0;

				switch (dpb.source_offset)
				{
				case 0:
					val = local_size.x;
					break;

				case 4:
					val = local_size.y;
					break;

				case 8:
					val = local_size.z;
					break;

				default:
					throw invalid_argument("Invalid source_offset for DATA_PARAMETER_LOCAL_WORK_SIZE");
				}

				set_param(
						dpb.offset,
						dpb.data_size,
						val,
						size, dst, capacity);
			}
			break;

		case iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT:
			{
				// printf("kernel argument\n");

				if (args.size() <= dpb.argument_number)
					throw invalid_argument("Missing kernel argument");

				if (try_set_int_arg<int32_t>(
							args[dpb.argument_number].get(),
							dpb.offset, dpb.data_size, size, dst, capacity))
				{
						break;
				}

				if (try_set_int_arg<uint32_t>(
							args[dpb.argument_number].get(),
							dpb.offset, dpb.data_size, size, dst, capacity))
				{
						break;
				}

				if (try_set_int_arg<int64_t>(
							args[dpb.argument_number].get(),
							dpb.offset, dpb.data_size, size, dst, capacity))
				{
						break;
				}

				if (try_set_int_arg<uint64_t>(
							args[dpb.argument_number].get(),
							dpb.offset, dpb.data_size, size, dst, capacity))
				{
						break;
				}

				throw invalid_argument("Invalid kernel argument number " +
						std::to_string(dpb.argument_number) +
						" has unsupported type");
			}

		case iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL:
			{
				if (args.size() <= dpb.argument_number)
					throw invalid_argument("Missing kernel argument");

				/* TODO: What is this? */
				uint32_t bt_entry = 0;
				for (unsigned i = 0; i < dpb.argument_number; i++)
				{
					if (
							dynamic_cast<const KernelArgPtr*>(args[i].get()) ||
							dynamic_cast<const KernelArgGEMName*>(args[i].get()))
					{
						bt_entry++;
					}
				}

				set_param(
						dpb.offset,
						dpb.data_size,
						bt_entry,
						size, dst, capacity);
			}
			break;

		default:
			throw invalid_argument("Unknown DataParameterBuffer in Kernel params: " +
					to_hex_string(dpb.type));
		}
	}

	for (auto& sgo : params.stateless_global_memory_object_kernel_arguments)
	{
		if (args.size() <= sgo.argument_number)
			throw invalid_argument("Missing kernel argument");

		// printf("DEBUG: %d %d %x\n",
		// 		(int) sgo.location_index,
		// 		(int) sgo.location_index2,
		// 		(unsigned) sgo.surface_state_heap_offset);

		HWInt::Gen9::RENDER_SURFACE_STATE rss;
		if (sgo.surface_state_heap_offset + sizeof(rss) > surface_state_size)
		{
			throw invalid_argument("Stateless global memory object's surface "
					"state does not lie in the surface state heap");
		}

		if (sgo.data_param_size != 8)
		{
			throw invalid_argument("Stateless global memory object with data "
					"param size != 8");
		}

		/* Find argument for surface state */
		auto arg = args[sgo.argument_number].get();
		auto arg_gem_name = dynamic_cast<const KernelArgGEMName*>(arg);
		uint64_t addr;

		if (arg_gem_name)
		{
			/* Relocate bo start address */
			relocs.push_back({arg_gem_name->handle(), sgo.data_param_offset});

			addr = 0;
		}
		else
		{
			/* Take address of pinned bo */
			memcpy(rss.data,
					surface_state_base + sgo.surface_state_heap_offset,
					sizeof(rss.data));

			addr = rss.get_surface_base_address();
		}

		set_param(
				sgo.data_param_offset,
				sgo.data_param_size,
				addr,
				size, dst, capacity);
	}

	// auto ptr = (const unsigned char*) dst;
	// for (unsigned i = 0; i < size && (i + 31) < capacity; i += 32)
	// {
	// 	printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x "
	// 			"%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
	// 			(unsigned) ptr[31], (unsigned) ptr[30], (unsigned) ptr[29], (unsigned) ptr[28],
	// 			(unsigned) ptr[27], (unsigned) ptr[26], (unsigned) ptr[25], (unsigned) ptr[24],
	// 			(unsigned) ptr[23], (unsigned) ptr[22], (unsigned) ptr[21], (unsigned) ptr[20],
	// 			(unsigned) ptr[19], (unsigned) ptr[18], (unsigned) ptr[17], (unsigned) ptr[16],
	// 			(unsigned) ptr[15], (unsigned) ptr[14], (unsigned) ptr[13], (unsigned) ptr[12],
	// 			(unsigned) ptr[11], (unsigned) ptr[10], (unsigned) ptr[9], (unsigned) ptr[8],
	// 			(unsigned) ptr[7], (unsigned) ptr[6], (unsigned) ptr[5], (unsigned) ptr[4],
	// 			(unsigned) ptr[3], (unsigned) ptr[2], (unsigned) ptr[1], (unsigned) ptr[0]);

	// 	ptr += 32;
	// }

	return size;
}

}
