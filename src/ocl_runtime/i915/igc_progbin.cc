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
#include <stdexcept>
#include "igc_progbin.h"
#include "ensure_types.h"
#include "macros.h"

using namespace std;

namespace OCL {

/* The unpadded/padded size division approach from intel-compute-runtime -
 * shared/offline_compiler/source/decoder/binary_decoder.cpp */
Heap::Heap(const char* bin, size_t unpadded_size, size_t size)
	: unpadded_size(unpadded_size), size(size)
{
	if (unpadded_size > size)
		throw invalid_argument("unpadded_size > size");

	if (size > 0)
	{
		buf = (char*) malloc(size*sizeof(char));
		if (!buf)
			throw bad_alloc();

		memcpy(buf, bin, size);
	}
	else
	{
		buf = nullptr;
	}
}

Heap::~Heap()
{
	if (buf)
		free(buf);
}

const char* Heap::ptr() const
{
	return buf;
}


/*************************** Read a program binary ****************************/
iOpenCL::SProgramBinaryHeader read_program_binary_header(const char*& bin, size_t& size)
{
	size_t constexpr SIZE = 7*4;

	/* Assuming SProgramBinaryHeader is packed due to 32bit alignment, check
	 * that the field count matches the one defined in the header file. */
	static_assert(SIZE == sizeof(iOpenCL::SProgramBinaryHeader));

	if (size < SIZE)
		throw invalid_argument("Program binary header too small");

	iOpenCL::SProgramBinaryHeader hdr;

	auto ptr = (const uint32_t*) bin;

	hdr.Magic = ptr[0];
	hdr.Version = ptr[1];
	hdr.Device = ptr[2];
	hdr.GPUPointerSizeInBytes = ptr[3];
	hdr.NumberOfKernels = ptr[4];
	hdr.SteppingId = ptr[5];
	hdr.PatchListSize = ptr[6];

	if (hdr.Magic != iOpenCL::MAGIC_CL)
		throw invalid_argument("Unknown binary format");

	if (hdr.Version != iOpenCL::CURRENT_ICBE_VERSION)
		throw invalid_argument("Unsupported binary format version");

	bin += SIZE;
	size -= SIZE;

	return hdr;
}

string to_string(const iOpenCL::SProgramBinaryHeader& hdr)
{
	char buf[1024];

	snprintf(buf, ARRAY_SIZE(buf) - 1,
			"ProgramBinaryHeader:\n"
"    Magic: 0x%08x\n"
"    Version: %d\n"
"    Device: 0x%04x\n"
"    GPUPointerSizeInBytes: %d\n"
"    NumberOfKernels: %d\n"
"    SteppingId: 0x%04x\n"
"    PatchListSize: %d\n",
			(int) hdr.Magic, (int) hdr.Version, (int) hdr.Device,
			(int) hdr.GPUPointerSizeInBytes, (int) hdr.NumberOfKernels,
			(int) hdr.SteppingId, (int) hdr.PatchListSize);

	buf[ARRAY_SIZE(buf) - 1] = '\0';
	return string(buf);
}


/**************************** Read a kernel binary ****************************/
/* Converting kernel binary headers to a string */
string to_string(const iOpenCL::SKernelBinaryHeaderGen9& hdr)
{
	char buf[1024];

	snprintf(buf, ARRAY_SIZE(buf) - 1,
			"KernelBinaryHeaderGen9:\n"
"    CheckSum: 0x%08x\n"
"    ShaderHashCode: 0x%016llx\n"
"    KernelNameSize: %d\n"
"    PatchListSize: %d\n"
"    KernelHeapSize: %d\n"
"    GeneralStateHeapSize: %d\n"
"    DynamicStateHeapSize: %d\n"
"    SurfaceStateHeapSize: %d\n"
"    KernelUnpaddedSize: %d\n",
			(int) hdr.CheckSum, (long long) hdr.ShaderHashCode,
			(int) hdr.KernelNameSize, (int) hdr.PatchListSize,
			(int) hdr.KernelHeapSize, (int) hdr.GeneralStateHeapSize,
			(int) hdr.DynamicStateHeapSize, (int) hdr.SurfaceStateHeapSize,
			(int) hdr.KernelUnpaddedSize);

	buf[ARRAY_SIZE(buf) - 1] = '\0';
	return string(buf);
}

/* Parsing binary kernel headers */
template<typename T>
T read_binary(const char*& bin)
{
	T val = *((const T*) bin);
	bin += sizeof(T);
	return val;
}

void read_kernel_binary_header_common(
		iOpenCL::SKernelBinaryHeaderCommon& kernel_hdr,
		const char*& bin, size_t& size)
{
	size_t constexpr SIZE = 8*4 + 1*8;
	static_assert(SIZE == sizeof(iOpenCL::SKernelBinaryHeaderCommon));

	if (size < SIZE)
		throw invalid_argument("Kernel binary header too small");

	size -= SIZE;

	/* SKernelBinaryHeader */
	kernel_hdr.CheckSum = read_binary<uint32_t>(bin);;
	kernel_hdr.ShaderHashCode = read_binary<uint64_t>(bin);
	kernel_hdr.KernelNameSize = read_binary<uint32_t>(bin);
	kernel_hdr.PatchListSize = read_binary<uint32_t>(bin);

	/* SKernelBinaryHeaderCommon */
	kernel_hdr.KernelHeapSize = read_binary<uint32_t>(bin);
	kernel_hdr.GeneralStateHeapSize = read_binary<uint32_t>(bin);
	kernel_hdr.DynamicStateHeapSize = read_binary<uint32_t>(bin);
	kernel_hdr.SurfaceStateHeapSize = read_binary<uint32_t>(bin);
	kernel_hdr.KernelUnpaddedSize = read_binary<uint32_t>(bin);

	if (kernel_hdr.KernelNameSize == 0)
		throw invalid_argument("KernelNameSize in kernel binary header is 0");
}

string read_kernel_name(const char*& bin, size_t& size, const iOpenCL::SKernelBinaryHeader& kernel_hdr)
{
	if (size < kernel_hdr.KernelNameSize)
		throw invalid_argument("Binary too short for kernel name");

	/* Make NUL bytes terminate the kernel name (it seems that the stored kernel
	 * name includes a terminating NUL character). */
	string name(bin, strnlen(bin, kernel_hdr.KernelNameSize));
	size -= kernel_hdr.KernelNameSize;
	bin += kernel_hdr.KernelNameSize;

	return name;
}

iOpenCL::SKernelBinaryHeaderGen9 read_kernel_binary_header_gen9(
	const char*& bin, size_t& size)
{
	iOpenCL::SKernelBinaryHeaderGen9 kernel_hdr;
	read_kernel_binary_header_common(kernel_hdr, bin, size);
	return kernel_hdr;
}


KernelParameters build_kernel_params(
		const iOpenCL::SProgramBinaryHeader& hdr,
		const iOpenCL::SKernelBinaryHeader& kernel_hdr)
{
	KernelParameters p;

	p.device = hdr.Device;
	p.gpu_pointer_size_in_bytes = hdr.GPUPointerSizeInBytes;
	p.stepping_id = hdr.SteppingId;

	p.checksum = kernel_hdr.CheckSum;
	p.shader_hash_code = kernel_hdr.ShaderHashCode;

	return p;
}


/* Adapted from intel-compute-runtime -
 * shared/offline_compiler/source/decoder/binary_decoder.cpp */
void read_kernel_patchlist(
		const char*& bin, size_t& size,
		const iOpenCL::SKernelBinaryHeader& kernel_hdr,
		KernelParameters& params)
{
	if (size < kernel_hdr.PatchListSize)
		throw invalid_argument("Patch list too small");

	auto patch_list_size = kernel_hdr.PatchListSize;
	while (patch_list_size > 0)
	{
		if (patch_list_size < 8)
			throw invalid_argument("Not enough remaining data for patch item header");

		auto token = read_binary<uint32_t>(bin);
		auto item_size = read_binary<uint32_t>(bin);

		if (patch_list_size < item_size)
			throw invalid_argument("Not enough remaining data for patch item");

		switch (token)
		{
		case iOpenCL::PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD:
			{
				static_assert(sizeof(iOpenCL::SPatchMediaInterfaceDescriptorLoad) == 8 + 4);
				if (item_size != 8 + 4 || params.media_interface_descriptor_load)
					throw invalid_argument("Failed to read patch item MediaInterfaceDescriptorLoad");

				params.media_interface_descriptor_load.emplace();
				params.media_interface_descriptor_load->data_offset = read_binary<uint32_t>(bin);
			}
			break;

		case iOpenCL::PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA:
			{
				static_assert(sizeof(iOpenCL::SPatchInterfaceDescriptorData) == 8 + 4*4);
				if (item_size != 8 + 4*4 || params.interface_descriptor_data)
					throw invalid_argument("Failed to read patch item InterfaceDescriptorData");

				params.interface_descriptor_data.emplace();
				params.interface_descriptor_data->offset = read_binary<uint32_t>(bin);
				params.interface_descriptor_data->sampler_state_offset = read_binary<uint32_t>(bin);
				params.interface_descriptor_data->kernel_offset = read_binary<uint32_t>(bin);
				params.interface_descriptor_data->binding_table_offset = read_binary<uint32_t>(bin);
			}
			break;

		case iOpenCL::PATCH_TOKEN_BINDING_TABLE_STATE:
			{
				static_assert(sizeof(iOpenCL::SPatchBindingTableState) == 8 + 3*4);
				if (item_size != 8 + 3*4 || params.binding_table_state)
					throw invalid_argument("Failed to read patch item BindingTableState");

				params.binding_table_state.emplace();
				params.binding_table_state->offset = read_binary<uint32_t>(bin);
				params.binding_table_state->count = read_binary<uint32_t>(bin);
				params.binding_table_state->surface_state_offset = read_binary<uint32_t>(bin);
			}
			break;

		case iOpenCL::PATCH_TOKEN_DATA_PARAMETER_BUFFER:
			{
				static_assert(sizeof(iOpenCL::SPatchDataParameterBuffer) == 8 + 8*4);
				if (item_size != 8 + 8*4)
					throw invalid_argument("Failed to read patch item DataParameterBuffer");

				KernelParameters::DataParameterBuffer dpb;
				dpb.type = read_binary<uint32_t>(bin);
				dpb.argument_number = read_binary<uint32_t>(bin);
				dpb.offset = read_binary<uint32_t>(bin);
				dpb.data_size = read_binary<uint32_t>(bin);
				dpb.source_offset = read_binary<uint32_t>(bin);
				dpb.location_index = read_binary<uint32_t>(bin);
				dpb.location_index2 = read_binary<uint32_t>(bin);
				dpb.is_emulation_argument = read_binary<uint32_t>(bin);

				params.data_parameter_buffers.push_back(dpb);
			}
			break;

		case iOpenCL::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT:
			{
				static_assert(sizeof(iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument) == 8 + 7*4);
				if (item_size != 8 + 7*4)
					throw invalid_argument("Failed to read patch item StatelessGlobalMemoryObjectKernelArgument");

				KernelParameters::StatelessGlobalMemoryObjectKernelArgument sgmoa;
				sgmoa.argument_number = read_binary<uint32_t>(bin);
				sgmoa.surface_state_heap_offset = read_binary<uint32_t>(bin);
				sgmoa.data_param_offset = read_binary<uint32_t>(bin);
				sgmoa.data_param_size = read_binary<uint32_t>(bin);
				sgmoa.location_index = read_binary<uint32_t>(bin);
				sgmoa.location_index2 = read_binary<uint32_t>(bin);
				sgmoa.is_emulation_argument = read_binary<uint32_t>(bin);

				params.stateless_global_memory_object_kernel_arguments.push_back(sgmoa);
			}
			break;

		case iOpenCL::PATCH_TOKEN_DATA_PARAMETER_STREAM:
			{
				static_assert(sizeof(iOpenCL::SPatchDataParameterStream) == 8 + 4);
				if (item_size != 8 + 4)
					throw invalid_argument("Failed to read patch item DataParameterStream");

				KernelParameters::DataParameterStream dps;
				dps.data_parameter_stream_size = read_binary<uint32_t>(bin);

				params.data_parameter_streams.push_back(dps);
			}
			break;

		case iOpenCL::PATCH_TOKEN_THREAD_PAYLOAD:
			{
				static_assert(sizeof(iOpenCL::SPatchThreadPayload) == 8 + 20*4);
				if (item_size != 8 + 20*4 || params.thread_payload)
					throw invalid_argument("Failed to read patch item ThreadPayload");

				params.thread_payload.emplace();
				params.thread_payload->header_present = read_binary<uint32_t>(bin);
				params.thread_payload->local_id_x_present = read_binary<uint32_t>(bin);
				params.thread_payload->local_id_y_present = read_binary<uint32_t>(bin);
				params.thread_payload->local_id_z_present = read_binary<uint32_t>(bin);
				params.thread_payload->local_id_flattened_present = read_binary<uint32_t>(bin);
				params.thread_payload->indirect_payload_storage = read_binary<uint32_t>(bin);
				params.thread_payload->unused_per_thread_constant_present = read_binary<uint32_t>(bin);
				params.thread_payload->get_local_id_present = read_binary<uint32_t>(bin);
				params.thread_payload->get_group_id_present = read_binary<uint32_t>(bin);
				params.thread_payload->get_global_offset_present = read_binary<uint32_t>(bin);
				params.thread_payload->stage_in_grid_origin_present = read_binary<uint32_t>(bin);
				params.thread_payload->stage_in_grid_size_present = read_binary<uint32_t>(bin);
				params.thread_payload->offset_to_skip_per_thread_data_load = read_binary<uint32_t>(bin);
				params.thread_payload->offset_to_skip_set_ffidgp = read_binary<uint32_t>(bin);
				params.thread_payload->pass_inline_data = read_binary<uint32_t>(bin);
				params.thread_payload->rt_stack_id_present = read_binary<uint32_t>(bin);
				params.thread_payload->generate_local_id = read_binary<uint32_t>(bin);
				params.thread_payload->emit_local_mask = read_binary<uint32_t>(bin);
				params.thread_payload->walk_order = read_binary<uint32_t>(bin);
				params.thread_payload->tile_y = read_binary<uint32_t>(bin);
			}
			break;

		case iOpenCL::PATCH_TOKEN_EXECUTION_ENVIRONMENT:
			{
				static_assert(sizeof(iOpenCL::SPatchExecutionEnvironment) == 8 + 31*4 + 1*8);
				if (item_size != 8 + 31*4 + 1*8 || params.execution_environment)
					throw invalid_argument("Failed to read patch item ExecutionEnvironment");

				params.execution_environment.emplace();
				params.execution_environment->required_work_group_size_x = read_binary<uint32_t>(bin);
				params.execution_environment->required_work_group_size_y = read_binary<uint32_t>(bin);
				params.execution_environment->required_work_group_size_z = read_binary<uint32_t>(bin);
				params.execution_environment->largest_compiled_simd_size = read_binary<uint32_t>(bin);
				params.execution_environment->compiled_sub_groups_number = read_binary<uint32_t>(bin);
				params.execution_environment->has_barriers = read_binary<uint32_t>(bin);
				params.execution_environment->disable_mid_thread_preemption = read_binary<uint32_t>(bin);
				params.execution_environment->compiled_simd8 = read_binary<uint32_t>(bin);
				params.execution_environment->compiled_simd16 = read_binary<uint32_t>(bin);
				params.execution_environment->compiled_simd32 = read_binary<uint32_t>(bin);
				params.execution_environment->has_device_enqueue = read_binary<uint32_t>(bin);
				params.execution_environment->may_access_undeclared_resource = read_binary<uint32_t>(bin);
				params.execution_environment->uses_fences_for_read_write_images = read_binary<uint32_t>(bin);
				params.execution_environment->uses_stateless_spill_fill = read_binary<uint32_t>(bin);
				params.execution_environment->uses_multi_scratch_spaces = read_binary<uint32_t>(bin);
				params.execution_environment->is_coherent = read_binary<uint32_t>(bin);
				params.execution_environment->is_initializer = read_binary<uint32_t>(bin);
				params.execution_environment->is_finalizer = read_binary<uint32_t>(bin);
				params.execution_environment->subgroup_independent_forward_progress_required =
					read_binary<uint32_t>(bin);

				params.execution_environment->compiled_for_greater_than_4gb_buffers = read_binary<uint32_t>(bin);
				params.execution_environment->num_grf_required = read_binary<uint32_t>(bin);
				params.execution_environment->workgroup_walk_order_dims = read_binary<uint32_t>(bin);
				params.execution_environment->has_global_atomics = read_binary<uint32_t>(bin);
				params.execution_environment->has_dpas = read_binary<uint32_t>(bin);
				params.execution_environment->has_rt_calls = read_binary<uint32_t>(bin);
				params.execution_environment->num_threads_required = read_binary<uint32_t>(bin);
				params.execution_environment->stateless_writes_count = read_binary<uint32_t>(bin);
				params.execution_environment->indirect_stateless_count = read_binary<uint32_t>(bin);
				params.execution_environment->use_bindless_mode = read_binary<uint32_t>(bin);
				params.execution_environment->has_stack_calls = read_binary<uint32_t>(bin);
				params.execution_environment->simd_info = read_binary<uint64_t>(bin);
				params.execution_environment->require_disable_eu_fusion = read_binary<uint32_t>(bin);
			}
			break;

		case iOpenCL::PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO:
			{
				static_assert(sizeof(iOpenCL::SPatchKernelAttributesInfo) == 8 + 4);
				if (item_size < 8 + 4 || params.kernel_attributes_info)
					throw invalid_argument("Failed to read patch item KernelAttributesInfo");

				params.kernel_attributes_info.emplace();
				auto attrs_size = read_binary<uint32_t>(bin);
				if (attrs_size != item_size - (8 + 4))
					throw invalid_argument("Failed to read patch item KernelAttributesInfo");

				params.kernel_attributes_info.emplace();
				params.kernel_attributes_info->attributes = string(bin, strnlen(bin, attrs_size));
				bin += attrs_size;
			}
			break;

		case iOpenCL::PATCH_TOKEN_KERNEL_ARGUMENT_INFO:
			{
				static_assert(sizeof(iOpenCL::SPatchKernelArgumentInfo) == 8 + 6*4);
				if (item_size < 8 + 6*4)
					throw invalid_argument("Failed to read patch item KernelArgumentInfo");

				KernelParameters::KernelArgumentInfo arg;
				arg.argument_number = read_binary<uint32_t>(bin);

				uint32_t address_qualifier_size = read_binary<uint32_t>(bin);
				uint32_t access_qualifier_size = read_binary<uint32_t>(bin);
				uint32_t argument_name_size = read_binary<uint32_t>(bin);
				uint32_t type_name_size = read_binary<uint32_t>(bin);
				uint32_t type_qualifier_size = read_binary<uint32_t>(bin);

				uint32_t total_size =
					address_qualifier_size +
					access_qualifier_size +
					argument_name_size +
					type_name_size +
					type_qualifier_size;

				if (total_size != item_size - (8 + 6*4))
					throw invalid_argument("Failed to read patch item KernelArgumentInfo");

				arg.address_qualifier = string(bin, strnlen(bin, address_qualifier_size));
				bin += address_qualifier_size;

				arg.access_qualifier = string(bin, strnlen(bin, access_qualifier_size));
				bin += access_qualifier_size;

				arg.argument_name = string(bin, strnlen(bin, argument_name_size));
				bin += argument_name_size;

				arg.type_name = string(bin, strnlen(bin, type_name_size));
				bin += type_name_size;

				arg.type_qualifier = string(bin, strnlen(bin, type_qualifier_size));
				bin += type_qualifier_size;

				params.kernel_argument_infos.push_back(arg);
			}
			break;

		case iOpenCL::PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE:
			{
				static_assert(sizeof(iOpenCL::SPatchAllocateLocalSurface) == 8 + 2*4);
				if (item_size != 8 + 2*4 || params.allocate_local_surface)
					throw invalid_argument("Failed to read patch item AllocateLocalSurface");

				KernelParameters::AllocateLocalSurface a;
				a.offset = read_binary<uint32_t>(bin);
				a.total_inline_local_memory_size = read_binary<uint32_t>(bin);

				params.allocate_local_surface = a;
			}
			break;

		default:
			throw invalid_argument("Unknown patch item with token \"" +
					std::to_string(token) + "\" and of size " +
					std::to_string(item_size) + ".");
		}

		patch_list_size -= item_size;
	}

	size -= kernel_hdr.PatchListSize;
}

}
