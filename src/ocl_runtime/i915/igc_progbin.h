/** Reading IGC's program binary format */
#ifndef __IGC_PROGBIN_H
#define __IGC_PROGBIN_H

#include <optional>
#include <string>
#include <vector>

#include <igfxfmid.h>
#include <ocl_igc_shared/executable_format/patch_list.h>
#include <ocl_igc_shared/executable_format/patch_g7.h>
#include <ocl_igc_shared/executable_format/patch_g8.h>
#include <ocl_igc_shared/executable_format/patch_g9.h>

static_assert(iOpenCL::CURRENT_ICBE_VERSION == 1081, "unsupported IGC version");

namespace OCL {

class Heap
{
protected:
	char* buf;

public:
	const size_t unpadded_size;
	const size_t size;

	/* Reads @param size bytes from bin */
	Heap(const char* bin, size_t unpadded_size, size_t padded_size);

	Heap(const Heap&) = delete;
	Heap& operator=(const Heap&) = delete;

	virtual ~Heap();

	const char* ptr() const;
};

struct KernelParameters final
{
	/* From program binary header */
	uint32_t device = 0;
	uint32_t gpu_pointer_size_in_bytes = 0;
	uint32_t stepping_id = 0;

	/* From kernel binary header */
	uint32_t checksum = 0;
	uint64_t shader_hash_code = 0;

	/* From patch tokens (adapted from
	 * ocl_igc_shared/executable_format/patch*.h) */
	struct MediaInterfaceDescriptorLoad
	{
		uint32_t data_offset = 0;
	};
	std::optional<MediaInterfaceDescriptorLoad> media_interface_descriptor_load;

	struct InterfaceDescriptorData
	{
		uint32_t offset = 0;
		uint32_t sampler_state_offset = 0;
		uint32_t kernel_offset = 0;
		uint32_t binding_table_offset = 0;
	};
	std::optional<InterfaceDescriptorData> interface_descriptor_data;

	struct BindingTableState
	{
		uint32_t offset = 0;
		uint32_t count = 0;
		uint32_t surface_state_offset = 0;
	};
	std::optional<BindingTableState> binding_table_state;

	struct DataParameterBuffer
	{
		uint32_t type = 0;
		uint32_t argument_number = 0;
		uint32_t offset = 0;
		uint32_t data_size = 0;
		uint32_t source_offset = 0;
		uint32_t location_index = 0;
		uint32_t location_index2 = 0;
		uint32_t is_emulation_argument = 0;
	};
	std::vector<DataParameterBuffer> data_parameter_buffers;

	struct StatelessGlobalMemoryObjectKernelArgument
	{
		uint32_t argument_number = 0;
		uint32_t surface_state_heap_offset = 0;
		uint32_t data_param_offset = 0;
		uint32_t data_param_size = 0;
		uint32_t location_index = 0;
		uint32_t location_index2 = 0;
		uint32_t is_emulation_argument = 0;
	};
	std::vector<StatelessGlobalMemoryObjectKernelArgument>
		stateless_global_memory_object_kernel_arguments;

	struct DataParameterStream
	{
		uint32_t data_parameter_stream_size = 0;
	};
	std::vector<DataParameterStream> data_parameter_streams;

	struct ThreadPayload
	{
		uint32_t header_present = 0;
		uint32_t local_id_x_present = 0;
		uint32_t local_id_y_present = 0;
		uint32_t local_id_z_present = 0;
		uint32_t local_id_flattened_present = 0;
		uint32_t indirect_payload_storage = 0;
		uint32_t unused_per_thread_constant_present = 0;
		uint32_t get_local_id_present = 0;
		uint32_t get_group_id_present = 0;
		uint32_t get_global_offset_present = 0;
		uint32_t stage_in_grid_origin_present = 0;
		uint32_t stage_in_grid_size_present = 0;
		uint32_t offset_to_skip_per_thread_data_load = 0;
		uint32_t offset_to_skip_set_ffidgp = 0;
		uint32_t pass_inline_data = 0;
		uint32_t rt_stack_id_present = 0;
		uint32_t generate_local_id = 0;
		uint32_t emit_local_mask = 0;
		uint32_t walk_order = 0;
		uint32_t tile_y = 0;
	};
	std::optional<ThreadPayload> thread_payload;

	struct ExecutionEnvironment
	{
		uint32_t required_work_group_size_x = 0;
		uint32_t required_work_group_size_y = 0;
		uint32_t required_work_group_size_z = 0;
		uint32_t largest_compiled_simd_size = 0;
		uint32_t compiled_sub_groups_number = 0;
		uint32_t has_barriers = 0;
		uint32_t disable_mid_thread_preemption = 0;
		uint32_t compiled_simd8 = 0;
		uint32_t compiled_simd16 = 0;
		uint32_t compiled_simd32 = 0;
		uint32_t has_device_enqueue = 0;
		uint32_t may_access_undeclared_resource = 0;
		uint32_t uses_fences_for_read_write_images = 0;
		uint32_t uses_stateless_spill_fill = 0;
		uint32_t uses_multi_scratch_spaces = 0;
		uint32_t is_coherent = 0;
		uint32_t is_initializer = 0;
		uint32_t is_finalizer = 0;
		uint32_t subgroup_independent_forward_progress_required = 0;
		uint32_t compiled_for_greater_than_4gb_buffers = 0;
		uint32_t num_grf_required = 0;
		uint32_t workgroup_walk_order_dims = 0;
		uint32_t has_global_atomics = 0;
		uint32_t has_dpas = 0;
		uint32_t has_rt_calls = 0;
		uint32_t num_threads_required = 0;
		uint32_t stateless_writes_count = 0;
		uint32_t indirect_stateless_count = 0;
		uint32_t use_bindless_mode = 0;
		uint32_t has_stack_calls = 0;
		uint64_t simd_info = 0;
		uint32_t require_disable_eu_fusion = 0;
	};
	std::optional<ExecutionEnvironment> execution_environment;

	/* Parsing adapted from intel-compute-runtime -
	 * shared/source/kernel/kernel_descriptor_from_patch_tokens.cpp */
	struct KernelAttributesInfo
	{
		std::string attributes;
	};
	std::optional<KernelAttributesInfo> kernel_attributes_info;

	/* Parsing and semantics adapted from intel-compute-runtime -
	 * shared/source/kernel/kernel_descriptor_from_patch_tokens.cpp and
	 * shared/source/device_binary_format/patchtokens_decoder.cpp
	 * and other locations */
	struct KernelArgumentInfo
	{
		uint32_t argument_number = 0;
		std::string address_qualifier;
		std::string access_qualifier;
		std::string argument_name;
		std::string type_name;
		std::string type_qualifier;
	};
	std::vector<KernelArgumentInfo> kernel_argument_infos;

	struct AllocateLocalSurface
	{
		uint32_t offset = 0;
		uint32_t total_inline_local_memory_size = 0;
	};
	std::optional<AllocateLocalSurface> allocate_local_surface;
};


iOpenCL::SProgramBinaryHeader read_program_binary_header(const char*& bin, size_t& size);
std::string to_string(const iOpenCL::SProgramBinaryHeader& hdr);
std::string to_string(const iOpenCL::SKernelBinaryHeaderGen9& hdr);
void read_kernel_binary_header_common(
		iOpenCL::SKernelBinaryHeaderCommon& kernel_hdr,
		const char*& bin, size_t& size);

std::string read_kernel_name(const char*& bin, size_t& size,
		const iOpenCL::SKernelBinaryHeader& kernel_hdr);

iOpenCL::SKernelBinaryHeaderGen9 read_kernel_binary_header_gen9(
		const char*& bin, size_t& size);

KernelParameters build_kernel_params(
		const iOpenCL::SProgramBinaryHeader& hdr,
		const iOpenCL::SKernelBinaryHeader& kernel_hdr);

void read_kernel_patchlist(
		const char*& bin, size_t& size,
		const iOpenCL::SKernelBinaryHeader& kernel_hdr,
		KernelParameters& params);
}

#endif /* __IGC_PROGBIN_H */
