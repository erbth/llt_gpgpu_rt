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
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <system_error>
#include <list>
#include "hash.h"
#include "i915_runtime_impl.h"
#include "i915_utils.h"
#include "igc_progbin.h"
#include "compiler/igc_interface.h"
#include "utils.h"
#include "macros.h"

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include "gen9_hw_int.h"

using namespace std;

namespace OCL
{
	using namespace HWInt;

	constexpr int GRF_SIZE = 32;

/* (Mandatory) methods of public interface */
I915Kernel::~I915Kernel()
{
}

I915PreparedKernel::~I915PreparedKernel()
{
}

I915RTE::~I915RTE()
{
}

unique_ptr<I915RTE> create_i915_rte(const char* device)
{
	return make_unique<I915RTEImpl>(device);
}

/* Actual Kernel class */
I915KernelImpl::I915KernelImpl(
		const string& name,
		const KernelParameters& params,
		unique_ptr<Heap>&& kernel_heap,
		unique_ptr<Heap>&& dynamic_state_heap,
		unique_ptr<Heap>&& surface_state_heap,
		const string& build_log)
	:
		name(name),
		params(params),
		kernel_heap(move(kernel_heap)),
		dynamic_state_heap(move(dynamic_state_heap)),
		surface_state_heap(move(surface_state_heap)),
		build_log(build_log)
{
}

I915KernelImpl::~I915KernelImpl()
{
}

string I915KernelImpl::get_build_log()
{
	return build_log;
}


/* Actual prepared kernel class */
I915PreparedKernelImpl::I915PreparedKernelImpl(I915RTEImpl& rte, shared_ptr<I915KernelImpl> kernel)
	: rte(rte), kernel(kernel)
{
}

I915PreparedKernelImpl::~I915PreparedKernelImpl()
{
}

template<typename T, const char* C>
void I915PreparedKernelImpl::add_argument_int(T val)
{
	unsigned index = args.size();

	for (auto& exp : kernel->params.kernel_argument_infos)
	{
		if (exp.argument_number == index)
		{
			/* Compare argument types */
			if (
					exp.address_qualifier == "__private" &&
					exp.access_qualifier == "NONE" &&
					exp.type_name == C &&
					exp.type_qualifier == "NONE")
			{
				args.push_back(make_unique<KernelArgInt<T>>(val));
				return;
			}

			throw invalid_argument(
					string("Argument type `") + C + "' does not match kernel signature (`"
					+ exp.type_name + "' expected for argument `" + exp.argument_name + "')");
		}
	}

	throw invalid_argument("No such kernel argument position");
}

void I915PreparedKernelImpl::add_argument(uint32_t val)
{
	static const char tid[] = "uint;4";
	add_argument_int<uint32_t, tid>(val);
}

void I915PreparedKernelImpl::add_argument(int32_t val)
{
	static const char tid[] = "int;4";
	add_argument_int<int32_t, tid>(val);
}

void I915PreparedKernelImpl::add_argument(uint64_t val)
{
	static const char tid[] = "ulong;8";
	add_argument_int<uint64_t, tid>(val);
}

void I915PreparedKernelImpl::add_argument(int64_t val)
{
	static const char tid[] = "long;8";
	add_argument_int<int64_t, tid>(val);
}

void I915PreparedKernelImpl::add_argument(void* ptr, size_t size)
{
	unsigned index = args.size();

	for (auto& exp : kernel->params.kernel_argument_infos)
	{
		if (exp.argument_number == index)
		{
			/* Compare argument types */
			if (
					exp.address_qualifier == "__global" &&
					exp.access_qualifier == "NONE" &&
					exp.type_name.size() >= 3 &&
					exp.type_name.find("*;8", exp.type_name.size() - 3) != decltype(exp.type_name)::npos &&
					exp.type_qualifier == "NONE")
			{
				args.push_back(make_unique<KernelArgPtr>(rte.get_page_size(), ptr, size));
				return;
			}

			throw invalid_argument(
					string("Argument `") + exp.type_name + "' is of non-pointer type `" +
						exp.type_name + "', but a pointer type is given");
		}

		// printf("DEBUG:\n");
		// printf("    argument_number: %d\n", (int) exp.argument_number);
		// printf("    address_qualifier: %s\n", exp.address_qualifier.c_str());
		// printf("    access_qualifier: %s\n", exp.access_qualifier.c_str());
		// printf("    argument_name: %s\n", exp.argument_name.c_str());
		// printf("    type_name: %s\n", exp.type_name.c_str());
		// printf("    type_qualifier: %s\n", exp.type_qualifier.c_str());
		// printf("\n\n");
	}

	throw invalid_argument("No such kernel argument position");
}

void I915PreparedKernelImpl::execute(NDRange global_size, NDRange local_size)
{
	/* Ensure that all arguments are bound */
	if (args.size() != kernel->params.kernel_argument_infos.size())
		throw runtime_error("Not all arguments where bound");


	/* Kernel parameters */
	if (!kernel->params.media_interface_descriptor_load)
		throw invalid_argument("Kernel has no MediaInterfaceDescriptorLoad param");


	/* Interpret supplied interface descriptor */
	/* Will be used by the RTE */
	Gen9::INTERFACE_DESCRIPTOR_DATA idesc;

	/* Supplied with the kernel (by the compiler) */
	Gen9::INTERFACE_DESCRIPTOR_DATA idesc_kernel;

	uint64_t kernel_idesc_offset = kernel->params.media_interface_descriptor_load->data_offset;
	if (!kernel->dynamic_state_heap || kernel->dynamic_state_heap->size < (
				kernel_idesc_offset + idesc_kernel.cnt_bytes))
	{
		throw invalid_argument("Kernel dynamic state heap too small for interface descriptor");
	}

	if (kernel->dynamic_state_heap->size != idesc_kernel.cnt_bytes)
	{
		throw invalid_argument("Kernel dynamic state heap contains more than "
				"an interface descriptor but this is not supported yet");
	}

	memcpy(
			idesc_kernel.data,
			kernel->dynamic_state_heap->ptr() + kernel_idesc_offset,
			idesc_kernel.cnt_bytes);

	uint64_t kernel_start_pointer = idesc_kernel.get_kernel_start_pointer() << 6;
	idesc.set_kernel_start_pointer(canonical_address(kernel_start_pointer) >> 6);
	idesc.set_denorm_mode(idesc_kernel.get_denorm_mode());
	idesc.set_floating_point_mode(idesc_kernel.get_floating_point_mode());

	uint64_t sampler_state_pointer = idesc_kernel.get_sampler_state_pointer() << 5;
	if (idesc_kernel.get_sampler_count() != Gen9::INTERFACE_DESCRIPTOR_DATA::SamplerCount_Nosamplersused)
		throw invalid_argument("Kernel uses sampelers but samplers are not supported yet");

	uint64_t binding_table_pointer = idesc_kernel.get_binding_table_pointer() << 5;
	uint32_t binding_table_entry_count = idesc_kernel.get_binding_table_entry_count();
	// printf("binding table entry count: %d\n", (int) binding_table_entry_count);

	uint32_t constant_urb_read_length = idesc_kernel.get_constant_urb_entry_read_length();
	uint32_t constant_urb_read_offset = idesc_kernel.get_constant_urb_entry_read_offset();

	idesc.set_rounding_mode(idesc_kernel.get_rounding_mode());
	// bool kernel_barrier_enable = idesc_kernel.get_barrier_enable();
	uint32_t kernel_slm_size = slm_size_from_idesc(idesc_kernel.get_shared_local_memory_size());
	// printf("barrier enable: %s, SLM size: %d\n",
	// 		(kernel_barrier_enable ? "yes" : "no"),
	// 		(int) kernel_slm_size);

	if (idesc_kernel.get_global_barrier_enable())
		throw invalid_argument("Kernel uses global barriers but global barriers are not supported");

	uint32_t cross_thread_constant_data_read_length =
		idesc_kernel.get_cross_thread_constant_data_read_length();

	if (kernel->params.interface_descriptor_data)
	{
		auto& idd = *(kernel->params.interface_descriptor_data);

		if (idd.offset != kernel_idesc_offset)
			throw invalid_argument("InterfaceDescriptorData param offset mismatch");

		if (idd.sampler_state_offset != sampler_state_pointer)
			throw invalid_argument("InterfaceDescriptorData param sampler_state_offset mismatch");

		if (idd.kernel_offset != kernel_start_pointer)
			throw invalid_argument("InterfaceDescriptorData param kernel_offset mismatch");

		if (idd.binding_table_offset != binding_table_pointer)
			throw invalid_argument("InterfaceDescriptorData param binding_table_offset mismatch");
	}


	/* State memory areas */
	size_t general_state_size = (1024 * rte.dev_info.max_cs_threads);
	size_t dynamic_state_size = 1024;
	size_t instruction_buffer_size = kernel->kernel_heap->size + kernel_start_pointer;
	size_t bindless_surface_size = 1024;

	if (dynamic_state_size < idesc.cnt_bytes)
		dynamic_state_size = idesc.cnt_bytes;

	general_state_size = rte.align_size_to_page(general_state_size);
	I915UserptrBo general_state_bo(rte, general_state_size);

	dynamic_state_size = rte.align_size_to_page(dynamic_state_size);
	I915UserptrBo dynamic_state_bo(rte, dynamic_state_size);

	instruction_buffer_size = rte.align_size_to_page(instruction_buffer_size);
	I915UserptrBo instruction_buffer_bo(rte, instruction_buffer_size);

	bindless_surface_size = rte.align_size_to_page(bindless_surface_size);
	I915UserptrBo bindless_surface_bo(rte, bindless_surface_size);

	size_t gp_bo_size = rte.align_size_to_page(sizeof(uint64_t));
	I915UserptrBo gp_bo(rte, gp_bo_size);


	/* Add missing fields in interface descriptor */
	idesc.set_single_program_flow(false);
	idesc.set_thread_priority(Gen9::INTERFACE_DESCRIPTOR_DATA::ThreadPriority_NormalPriority);
	idesc.set_illegal_opcode_exception_enable(false);
	idesc.set_mask_stack_exception_enable(false);
	idesc.set_software_exception_enable(false);
	idesc.set_sampler_count(Gen9::INTERFACE_DESCRIPTOR_DATA::SamplerCount_Nosamplersused);
	idesc.set_barrier_enable(true);


	/* Copy surface state heap */
	size_t surface_state_size = 1024;

	if (kernel->surface_state_heap)
	{
		if (surface_state_size < kernel->surface_state_heap->size)
			surface_state_size = kernel->surface_state_heap->size;
	}

	surface_state_size = rte.align_size_to_page(surface_state_size);
	I915UserptrBo surface_state_bo(rte, surface_state_size);

	if (kernel->surface_state_heap)
	{
		memcpy(
				surface_state_bo.ptr(),
				kernel->surface_state_heap->ptr(),
				kernel->surface_state_heap->size);
	}

	/* Validate binding table */
	list<I915UserptrBo> arg_bos;

	if (binding_table_entry_count > 0)
	{
		if (!kernel->params.binding_table_state)
		{
			throw invalid_argument("Kernel requries a binding table but has "
					"no binding table state param");
		}

		auto& param_bts = *(kernel->params.binding_table_state);
		if (
				param_bts.offset != binding_table_pointer ||
				param_bts.count != binding_table_entry_count ||
				param_bts.surface_state_offset != 0)
		{
			throw invalid_argument("Kernel has an unsupported binding table state param");
		}

		if (!kernel->surface_state_heap)
		{
			throw invalid_argument("Kernel requires a binding table but has "
					"no surface state heap");
		}

		Gen9::BINDING_TABLE_STATE bts;
		Gen9::RENDER_SURFACE_STATE rss;

		if (binding_table_pointer + binding_table_entry_count * bts.cnt_bytes >
				kernel->surface_state_heap->size)
		{
			throw invalid_argument("Not all binding table entries are located "
					"in the surface state heap");
		}

		size_t cnt_buffer_args = 0;
		for (auto& arg : args)
		{
			if (dynamic_cast<KernelArgPtr*>(arg.get()))
				cnt_buffer_args++;
		}

		if (cnt_buffer_args != binding_table_entry_count)
			throw runtime_error("Kernel binding table entry count != buffer-like kernel argument count");

		auto i_kernel_arg = args.begin();

		for (unsigned i = 0; i < binding_table_entry_count; i++)
		{
			while (dynamic_cast<KernelArgPtr*>(i_kernel_arg->get()) == nullptr)
				i_kernel_arg++;

			memcpy(
					bts.data,
					(char*) surface_state_bo.ptr() + binding_table_pointer,
					bts.cnt_bytes);

			uint64_t surface_state_pointer = bts.get_surface_state_pointer();
			if (surface_state_pointer + rss.cnt_bytes > kernel->surface_state_heap->size)
			{
				throw invalid_argument("Surface state block does not fit in "
						"supplied surface state heap");
			}

			memcpy(rss.data, (char*) surface_state_bo.ptr() + surface_state_pointer, rss.cnt_bytes);

			/* Validate RENDER_SURFACE_STATE */
			if (rss.get_surface_type() != Gen9::RENDER_SURFACE_STATE::SurfaceType_SURFTYPE_BUFFER)
				throw invalid_argument("Surface with type != buffer");

			if (rss.get_surface_array())
				throw invalid_argument("Surface array");

			if (rss.get_surface_format() != 0xff)
			{
				throw invalid_argument("Invalid surface format: 0x" +
						to_hex_string(rss.get_surface_format()));
			}

			if (rss.get_surface_horizontal_alignment() > 3 || 
					rss.get_surface_vertical_alignment() > 3)
			{
				throw invalid_argument("Invalid surface alignment");
			}

			if (rss.get_tile_mode() != 0)
				throw invalid_argument("Invalid surface tiling mode");

			if (rss.get_vertical_line_stride() != 0)
				throw invalid_argument("Invalid surface vertical line stride");

			if (rss.get_vertical_line_stride_offset() != 0)
				throw invalid_argument("Invalid surface vertical line strice offset");

			if (rss.get_sampler_l2_bypass_mode_disable())
				throw invalid_argument("Invalid: surface L2 bypass mode disabled");

			if (rss.get_render_cache_read_write_mode() == 1)
				throw invalid_argument("surface read-write cache enabled");

			if (rss.get_media_boundary_pixel_mode() !=
					Gen9::RENDER_SURFACE_STATE::MediaBoundaryPixelMode_NORMAL_MODE)
			{
				throw invalid_argument("Invalid surface media boundary pixel mode");
			}

			// printf("DEBUG: %d\n", (int) rss.get_mocs());
			// if (rss.get_mocs() != I915_MOCS_CACHED)
			// 	throw invalid_argument("Invalid surface MOCS");

			if (rss.get_memory_compression_enable())
				throw invalid_argument("Surface memory compression enabled");

			if (rss.get_auxiliary_surface_mode() !=
					Gen9::RENDER_SURFACE_STATE::AuxiliarySurfaceMode_AUX_NONE)
			{
				throw invalid_argument("Surface with auxiliary surface mode != None");
			}

			/* Bind surface to buffer-argument */
			auto& kernel_arg = static_cast<KernelArgPtr&>(*(*i_kernel_arg));
			rss.set_surface_base_address(canonical_address(kernel_arg.ptr()));
			arg_bos.emplace_back(rte, kernel_arg.ptr(), kernel_arg.size());

			if (kernel_arg.size() < 1)
				throw invalid_argument("Kernel buffer argument with size < 1");

			uint32_t surface_size = kernel_arg.size() - 1;
			rss.set_width(surface_size & 0x7f);
			rss.set_height((surface_size >> 7) & 0x3fff);
			rss.set_depth((surface_size >> 21) & 0x7ff);

			memcpy((char*) surface_state_bo.ptr() + surface_state_pointer, rss.data, rss.cnt_bytes);
		}

		idesc.set_binding_table_pointer(binding_table_pointer >> 5);
	}

	idesc.set_binding_table_entry_count(binding_table_entry_count);


	/* Check other kernel params */
	if (kernel->params.kernel_attributes_info &&
			kernel->params.kernel_attributes_info->attributes.size() > 0)
	{
		throw invalid_argument("Kernel has an attributes info param but that "
				"is not supported yet");
	}

	if (kernel->params.allocate_local_surface)
	{
		throw invalid_argument("Kernel requires a local surface but that is not"
				"supported yet");
	}


	if (!kernel->params.execution_environment)
		throw invalid_argument("ExecutionEnvironment missing from kernel params");

	auto& exe = *(kernel->params.execution_environment);

	/* Setup execution environment */
	if (exe.may_access_undeclared_resource != 0)
		throw invalid_argument("Kernel may access undeclared resource");

	if (exe.uses_fences_for_read_write_images != 0)
	{
		throw invalid_argument("Kernel uses fences for image access, but "
				"fences are not supported yet.");
	}

	if (exe.uses_stateless_spill_fill != 0)
	{
		throw invalid_argument("Kernel uses stateless-spill fill, but is not "
				"supported yet");
	}

	if (exe.uses_multi_scratch_spaces != 0)
	{
		throw invalid_argument("Kernel uses multi scratch spaces, but is not "
				"supported yet");
	}

	if (exe.is_coherent != 0)
		throw invalid_argument("Kernel is coherent");

	if (exe.is_initializer != 0)
		throw invalid_argument("Kernel is initialzier");

	if (exe.is_finalizer != 0)
		throw invalid_argument("Kernel is finalizer");

	if (exe.has_global_atomics != 0)
		throw invalid_argument("Kernel has global atomics");

	if (exe.has_device_enqueue != 0)
		throw invalid_argument("Kernel has device enqueue");

	if (exe.stateless_writes_count != 0)
		throw invalid_argument("Kernel has stateless writes");

	if (exe.use_bindless_mode != 0)
		throw invalid_argument("Kernel has bindless_mode != 0");


	/* Distribute threads */
	/* Choose a SIMD size */
	int simd_size = exe.largest_compiled_simd_size;
	if (simd_size != 8 && simd_size != 16 && simd_size != 32)
	{
		throw invalid_argument("Unsupported largest compiled SIMD size: " +
				std::to_string(simd_size));
	}

	if (local_size.x < 1 || local_size.y < 1 || local_size.z < 1)
		throw invalid_argument("Invalid work group size");

	int cnt_ocl_threads = local_size.x * local_size.y * local_size.z;
	if (cnt_ocl_threads > 1024)
		throw invalid_argument("At most 1024 threads per work group are supported");

	uint32_t cnt_threads;
	uint32_t threads_x;

	for (;; simd_size /= 2)
	{
		if (simd_size < 8)
			throw invalid_argument("Could not choose a SIMD-channel configuration");

		if (simd_size == 32 && exe.compiled_simd32 != 1)
			continue;

		if (simd_size == 16 && exe.compiled_simd16 != 1)
			continue;

		if (simd_size == 8 && exe.compiled_simd8 != 1)
			continue;

		if (local_size.x % simd_size != 0)
			continue;

		threads_x = local_size.x / simd_size;
		cnt_threads = threads_x * local_size.y * local_size.z;

		if (cnt_threads > rte.dev_info.max_cs_threads)
			continue;

		break;
	}

	if (simd_size == 32 && cnt_threads > 32)
	{
		throw invalid_argument("simd_size is 32 and more than 32 dispatches "
				"in thread group");
	}
	else if (simd_size != 32 && cnt_threads > 64)
	{
		throw invalid_argument("more than 64 dispatches in thread group "
				"(simd_size is < 32)");
	}

	// printf("Chosen SIMD-size: %d, %dx%dx%d threads, %d threads total\n",
	// 		(int) simd_size,
	// 		(int) threads_x,
	// 		(int) local_size.y,
	// 		(int) local_size.z,
	// 		(int) cnt_threads);


	if (
			global_size.x % local_size.x != 0 ||
			global_size.y % local_size.y != 0 ||
			global_size.z % local_size.z != 0)
	{
		throw invalid_argument("Global sizes must be multiples of local sizes");
	}

	NDRange thread_groups(
			global_size.x / local_size.x,
			global_size.y / local_size.y,
			global_size.z / local_size.z);

	idesc.set_number_of_threads_in_gpgpu_thread_group(cnt_threads);


	/* Setup CURBE data */
	if (!kernel->params.thread_payload)
		throw invalid_argument("Kernel has no ThreadPayload param");

	auto& tp = *(kernel->params.thread_payload);

	if (tp.indirect_payload_storage != 1)
		throw invalid_argument("Kernel does not use indirect payload storate");

	if (tp.offset_to_skip_per_thread_data_load != 0)
		throw invalid_argument("Kernel's offset_to_skip_per_thread_data_load != 0");

	if (tp.offset_to_skip_set_ffidgp != 0)
		throw invalid_argument("Kernel's offset_to_skip_set_ffidgp != 0");

	if (tp.pass_inline_data != 0)
		throw invalid_argument("Kernel's pass_inline_data != 0");

	if (tp.local_id_flattened_present != 0)
		throw invalid_argument("Kernel uses flattened local id");

	if (constant_urb_read_offset != 0)
		throw invalid_argument("Kernel param for constant URB entry read offset != 0");

	auto cross_thread_size_bytes = cross_thread_constant_data_read_length * 32;
	DynamicBuffer<char> indirect_data(cross_thread_size_bytes);

	/* Build cross-thread data */
	build_cross_thread_data(
			kernel->params,
			NDRange(0, 0, 0),
			local_size,
			args,
			(const char*) surface_state_bo.ptr(), surface_state_size,
			indirect_data.ptr(), cross_thread_size_bytes);


	/* Build per-thread data */
	size_t local_id_cnt = tp.local_id_x_present +
		tp.local_id_y_present + tp.local_id_z_present;

	if (local_id_cnt != 0 && local_id_cnt != 3)
	{
		throw invalid_argument("Only none or all local ids are supported for "
				"thread payload yet.");
	}

	size_t local_id_size_bytes = GRF_SIZE * (simd_size == 32 ? 2 : 1);
	size_t per_thread_size_bytes = local_id_cnt * local_id_size_bytes;

	if (tp.unused_per_thread_constant_present > 0)
		per_thread_size_bytes += GRF_SIZE;

	indirect_data.ensure_size(cross_thread_size_bytes + per_thread_size_bytes * cnt_threads);

	char* per_thread_ptr = indirect_data.ptr() + cross_thread_size_bytes;

	uint16_t x = 0;
	uint16_t y = 0;
	uint16_t z = 0;

	for (unsigned i = 0; i < cnt_threads; i++)
	{
		if (local_id_cnt > 0)
		{
			auto local_id_x = (uint16_t*) (per_thread_ptr);
			auto local_id_y = (uint16_t*) (per_thread_ptr + local_id_size_bytes);
			auto local_id_z = (uint16_t*) (per_thread_ptr + local_id_size_bytes*2);

			for (int j = 0; j < simd_size; j++)
			{
				local_id_x[j] = x;
				local_id_y[j] = y;
				local_id_z[j] = z;

				x += 1;
				y += x / local_size.x;
				x = x % local_size.x;
				z += y / local_size.y;
				y = y % local_size.y;
			}

			per_thread_ptr += local_id_size_bytes*3;
		}

		if (tp.unused_per_thread_constant_present > 0)
			per_thread_ptr += GRF_SIZE;
	}

	// printf("DEBUG header_present: %d\n", (int) tp.header_present);
	// printf("DEBUG local_id_x_present: %d\n", (int) tp.local_id_x_present);
	// printf("DEBUG local_id_y_present: %d\n", (int) tp.local_id_y_present);
	// printf("DEBUG local_id_z_present: %d\n", (int) tp.local_id_z_present);
	// printf("DEBUG local_id_flattened_present: %d\n", (int) tp.local_id_flattened_present);
	// printf("DEBUG indirect_payload_storage: %d\n", (int) tp.indirect_payload_storage);
	// printf("DEBUG unused_per_thread_constant_present: %d\n", (int) tp.unused_per_thread_constant_present);
	// printf("DEBUG get_local_id_present: %d\n", (int) tp.get_local_id_present);
	// printf("DEBUG get_group_id_present: %d\n", (int) tp.get_group_id_present);
	// printf("DEBUG get_global_offset_present: %d\n", (int) tp.get_global_offset_present);
	// printf("DEBUG stage_in_grid_origin_present: %d\n", (int) tp.stage_in_grid_origin_present);
	// printf("DEBUG stage_in_grid_size_present: %d\n", (int) tp.stage_in_grid_size_present);
	// printf("DEBUG offset_to_skip_per_thread_data_load: %d\n", (int) tp.offset_to_skip_per_thread_data_load);
	// printf("DEBUG offset_to_skip_set_ffidgp: %d\n", (int) tp.offset_to_skip_set_ffidgp);
	// printf("DEBUG pass_inline_data: %d\n", (int) tp.pass_inline_data);


	if (constant_urb_read_length != 0)
		throw invalid_argument("constant_urb_read_length from patch tokens != 0");

	constant_urb_read_length = DIV_ROUND_UP(per_thread_size_bytes, 32);

	idesc.set_constant_urb_entry_read_length(constant_urb_read_length);
	idesc.set_constant_urb_entry_read_offset(constant_urb_read_offset);
	idesc.set_cross_thread_constant_data_read_length(cross_thread_constant_data_read_length);

	// printf("const urb read length: %d / cross thread: %d\n",
	// 		(int) constant_urb_read_length,
	// 		(int) cross_thread_constant_data_read_length);

	uint32_t indirect_data_length =
		constant_urb_read_length * 32 * cnt_threads +
		cross_thread_constant_data_read_length * 32;


	/* From SKL PRM 2a, p. 488: "the total size of indirect data must be less
	 * than 63,488 (2048 URB lines - 64 lines for interface Descriptors)" */
	if (indirect_data_length >= 63488)
		throw invalid_argument("indirect_data_length too large");

	size_t indirect_object_size = indirect_data_length;
	indirect_object_size = rte.align_size_to_page(indirect_object_size);
	I915UserptrBo indirect_object_bo(rte, indirect_object_size);

	memset(indirect_object_bo.ptr(), 0, indirect_object_bo.size());

	memcpy(indirect_object_bo.ptr(),
		indirect_data.ptr(),
		cross_thread_size_bytes + per_thread_size_bytes * cnt_threads);


	/* Allocate SLM */
	if (kernel_slm_size > 0)
		throw invalid_argument("Kernel uses SLM but SLM is not supported yet");

	uint32_t slm_size = 0;
	idesc.set_shared_local_memory_size(slm_size_to_idesc(slm_size));


	/* Copy kernel code */
	/* To clarify: Does the instruction heap contain the kernel offset or not?
	 * */
	if (kernel_start_pointer != 0)
		throw runtime_error("kernel_start_pointer != 0 not implemented yet.");

	memcpy(
			(char*) instruction_buffer_bo.ptr() + kernel_start_pointer,
			kernel->kernel_heap->ptr(),
			kernel->kernel_heap->size);


	/* Copy interface descriptor to dynamic state heap */
	memcpy(dynamic_state_bo.ptr(), idesc.data, idesc.cnt_bytes);


	/* Build second batch buffer */
	vector<unique_ptr<I915RingCmd>> cmds2;

	{
		cmds2.push_back(make_unique<Gen9::CmdMediaStateFlush>());
	}

	{
		auto cmd = make_unique<Gen9::CmdMediaInterfaceDescriptorLoad>();
		static_assert(idesc.cnt_bytes == 32);
		cmd->interface_descriptor_total_length = 32;
		cmds2.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdGpgpuWalker>();

		cmd->predicate_enable = false;
		cmd->indirect_parameter_enable = false;
		cmd->interface_descriptor_offset = 0;
		cmd->indirect_data_length = indirect_data_length;
		cmd->indirect_data_start_address = 0;
		cmd->thread_width_counter_maximum = threads_x - 1;
		cmd->thread_height_counter_maximum = local_size.y - 1;
		cmd->thread_depth_counter_maximum = local_size.z - 1;
		cmd->simd_size =
			simd_size == 32 ? Gen9::CmdGpgpuWalker::SIMD32 :
			simd_size == 16 ? Gen9::CmdGpgpuWalker::SIMD16 :
			Gen9::CmdGpgpuWalker::SIMD8;

		cmd->thread_group_id_starting_x = 0;
		cmd->thread_group_id_x_dimension = thread_groups.x;
		cmd->thread_group_id_starting_y = 0;
		cmd->thread_group_id_y_dimension = thread_groups.y;
		cmd->thread_group_id_starting_resume_z = 0;
		cmd->thread_group_id_z_dimension = thread_groups.z;
		cmd->right_execution_mask = 0xffffffff;
		cmd->bottom_execution_mask = 0xffffffff;

		cmds2.push_back(move(cmd));
	}

	{
		cmds2.push_back(make_unique<Gen9::CmdMediaStateFlush>());
	}

	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->command_streamer_stall_enable = true;
		cmds2.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->command_streamer_stall_enable = true;
		cmd->dc_flush_enable = true;
		cmd->post_sync_operation = Gen9::CmdPipeControl::WriteImmediateData;
		cmd->address = canonical_address(gp_bo.ptr()) >> 2;
		cmd->immediate_data = 0x1;
		cmds2.push_back(move(cmd));
	}

	{
		cmds2.push_back(make_unique<Gen9::CmdMiBatchBufferEnd>());
		cmds2.push_back(make_unique<Gen9::CmdMiNoop>());
	}

	/* Copy to a Bo */
	size_t bb2_bo_size = 0;
	for (auto& cmd : cmds2)
		bb2_bo_size += cmd->bin_size();

	I915UserptrBo bb2(rte, bb2_bo_size);
	auto bb2_ptr = (char*) bb2.ptr();
	for (auto& cmd : cmds2)
		bb2_ptr += cmd->bin_write(bb2_ptr);


	/* Build first batch buffer */
	vector<unique_ptr<I915RingCmd>> cmds;
	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->command_streamer_stall_enable = true;
		cmd->render_target_cache_flush_enable = true;
		cmd->dc_flush_enable = true;
		cmd->depth_cache_flush_enable = true;
		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->command_streamer_stall_enable = true;
		cmd->texture_cache_invalidation_enable = true;
		cmd->constant_cache_invalidation_enable = true;
		cmd->state_cache_invalidation_enable = true;
		cmd->instruction_cache_invalidate_enable = true;
		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdPipelineSelect>();
		cmd->pipeline_selection = Gen9::CmdPipelineSelect::GPGPU;
		cmd->media_sampler_dop_clock_gate_enable = true;
		cmd->mask_bits = 0x13;
		cmds.push_back(move(cmd));
	}

	{
		Gen9::REG_L3CNTLREG reg;
		reg.set_slm_enable(true);

		/* TODO: Don't use fixed values here */
		reg.set_urb_allocation(0x10);
		reg.set_all_allocation(0x30);

		auto cmd = make_unique<Gen9::CmdMiLoadRegisterImm>();
		cmd->register_offset = reg.address >> 2;
		cmd->data_dword = reg.data[0];
		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->command_streamer_stall_enable = true;
		cmd->render_target_cache_flush_enable = true;
		cmd->dc_flush_enable = true;
		cmd->depth_cache_flush_enable = true;
		cmds.push_back(move(cmd));
	}

	{
		/* TODO: Don't use fixed values here */
		auto cmd = make_unique<Gen9::CmdMediaVfeState>();
		cmd->scratch_space_base_pointer = 0x0;
		cmd->stack_size = 0;
		cmd->per_thread_scratch_space = 0;
		cmd->maximum_number_of_threads = rte.dev_info.max_cs_threads - 1;
		cmd->number_of_urb_entries = 1;
		cmd->urb_entry_allocation_size = 1922;
		cmds.push_back(move(cmd));
	}

	{
		Gen9::REG_CS_CHICKEN1 reg;
		reg.set_replay_mode(Gen9::REG_CS_CHICKEN1::ReplayMode_MidcmdbufferPreemption);

		auto cmd= make_unique<Gen9::CmdMiLoadRegisterImm>();
		cmd->register_offset = reg.address >> 2;
		cmd->data_dword = reg.data[0];
		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->texture_cache_invalidation_enable = true;
		cmd->dc_flush_enable = true;
		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdStateBaseAddress>();

		cmd->general_state_base_address = canonical_address(general_state_bo.ptr()) >> 12;
		cmd->general_state_mocs = I915_MOCS_UNCACHED << 1;
		cmd->general_state_base_address_modify_enable = true;
		cmd->general_state_buffer_size = DIV_ROUND_UP(general_state_size, 4096);
		cmd->general_state_buffer_size_modify_enable = true;

		cmd->stateless_data_port_access_mocs = I915_MOCS_CACHED << 1;

		cmd->surface_state_base_address = canonical_address(surface_state_bo.ptr()) >> 12;
		cmd->surface_state_mocs = I915_MOCS_UNCACHED << 1;
		cmd->surface_state_base_address_modify_enable = true;

		cmd->dynamic_state_base_address = canonical_address(dynamic_state_bo.ptr()) >> 12;
		cmd->dynamic_state_mocs = I915_MOCS_UNCACHED << 1;
		cmd->dynamic_state_base_address_modify_enable = true;
		cmd->dynamic_state_buffer_size = DIV_ROUND_UP(dynamic_state_size, 4096);
		cmd->dynamic_state_buffer_size_modify_enable = true;

		cmd->indirect_object_base_address = canonical_address(indirect_object_bo.ptr()) >> 12;
		cmd->indirect_object_mocs = I915_MOCS_UNCACHED << 1;
		cmd->indirect_object_base_address_modify_enable = true;
		cmd->indirect_object_buffer_size = DIV_ROUND_UP(indirect_object_size, 4096);
		cmd->indirect_object_buffer_size_modify_enable = true;

		cmd->instruction_base_address = canonical_address(instruction_buffer_bo.ptr()) >> 12;
		cmd->instruction_mocs = I915_MOCS_CACHED << 1;
		cmd->instruction_base_address_modify_enable = true;
		cmd->instruction_buffer_size = DIV_ROUND_UP(instruction_buffer_size, 4096);
		cmd->instruction_buffer_size_modify_enable = true;

		cmd->bindless_surface_state_base_address = canonical_address(bindless_surface_bo.ptr()) >> 12;
		cmd->bindless_surface_state_mocs = I915_MOCS_UNCACHED << 1;
		cmd->bindless_surface_state_base_address_modify_enable = true;
		// cmd->bindless_surface_state_size = 0;

		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdPipeControl>();
		cmd->command_streamer_stall_enable = true;
		cmds.push_back(move(cmd));
	}

	{
		auto cmd = make_unique<Gen9::CmdMiBatchBufferStart>();
		cmd->address_space_indicator = Gen9::CmdMiBatchBufferStart::ASI_PPGTT;
		cmd->batch_buffer_start_address = canonical_address(bb2.ptr()) >> 2;
		cmds.push_back(move(cmd));
	}

	{
		cmds.push_back(make_unique<Gen9::CmdMiNoop>());
	}

	/* Copy to a Bo */
	size_t bb_bo_size = 0;
	for (auto& cmd : cmds)
		bb_bo_size += cmd->bin_size();

	I915UserptrBo bb(rte, bb_bo_size);
	auto bb_ptr = (char*) bb.ptr();
	for (auto& cmd : cmds)
		bb_ptr += cmd->bin_write(bb_ptr);

	/* Execute Bo */
	vector<pair<uint32_t, void*>> bos;

	for (auto& bo : arg_bos)
		bos.emplace_back(bo.handle(), bo.ptr());

	bos.emplace_back(general_state_bo.handle(), general_state_bo.ptr());
	bos.emplace_back(surface_state_bo.handle(), surface_state_bo.ptr());
	bos.emplace_back(dynamic_state_bo.handle(), dynamic_state_bo.ptr());
	bos.emplace_back(indirect_object_bo.handle(), indirect_object_bo.ptr());
	bos.emplace_back(instruction_buffer_bo.handle(), instruction_buffer_bo.ptr());
	bos.emplace_back(bindless_surface_bo.handle(), bindless_surface_bo.ptr());
	bos.emplace_back(gp_bo.handle(), gp_bo.ptr());

	bos.emplace_back(bb2.handle(), bb2.ptr());
	bos.emplace_back(bb.handle(), bb.ptr());

	volatile uint64_t* sync_ptr = (uint64_t*) gp_bo.ptr();
	*sync_ptr = 0;

	gem_execbuffer2(rte.fd, rte.ctx_id, bos, bb_bo_size);

	/* Wait for GPU */
	/* This IOCTL causes a reasonably high power consumption according to
	 * intel_gpu_top - maybe verify with monitoring chip power at some point...
	 * */
	// gem_wait(rte.fd, bb.handle(), 500 * 1000 * 1000);

	for (;;)
	{
		if (*sync_ptr == 1)
			break;

		usleep(500);
	}
}


/* Adapted from intel-compute-runtime -
 * shared/offline_compiler/source/decoder/binary_decoder.cpp */
shared_ptr<I915KernelImpl> I915KernelImpl::read_kernel(
		const char* bin, size_t size, const string& name, const string& build_log)
{
	shared_ptr<I915KernelImpl> kernel;

	auto hdr = read_program_binary_header(bin, size);

	if (hdr.PatchListSize != 0)
	{
		throw runtime_error("Program has patch tokens but patch tokens are "
				"not supported at program level yet.");
	}

	/* Parse kernels and find ours */
	for (uint32_t i = 0; i < hdr.NumberOfKernels; i++)
	{
		switch (hdr.Device)
		{
		case IGFX_GEN9_CORE:
			{
				auto kernel_hdr = read_kernel_binary_header_gen9(bin, size);
				auto kernel_data_start = bin;

				auto kernel_name = read_kernel_name(bin, size, kernel_hdr);

				/* Read heaps */
				if (kernel_hdr.GeneralStateHeapSize != 0)
				{
					throw invalid_argument("Read a kernel with general state "
							"heap size != 0, which is not supported");
				}

				size_t heaps_size =
					kernel_hdr.KernelHeapSize +
					kernel_hdr.GeneralStateHeapSize +
					kernel_hdr.DynamicStateHeapSize +
					kernel_hdr.SurfaceStateHeapSize;

				if (size < heaps_size)
					throw invalid_argument("Kernel heaps too small.");

				auto kernel_heap = make_unique<Heap>(
						bin,
						kernel_hdr.KernelUnpaddedSize,
						kernel_hdr.KernelHeapSize);

				bin += kernel_hdr.KernelHeapSize;

				auto dynamic_state_heap = make_unique<Heap>(
						bin,
						kernel_hdr.DynamicStateHeapSize,
						kernel_hdr.DynamicStateHeapSize);

				bin += kernel_hdr.DynamicStateHeapSize;

				auto surface_state_heap = make_unique<Heap>(
						bin,
						kernel_hdr.SurfaceStateHeapSize,
						kernel_hdr.SurfaceStateHeapSize);

				bin += kernel_hdr.SurfaceStateHeapSize;

				size -= heaps_size;

				/* Read patchlist */
				auto params = build_kernel_params(hdr, kernel_hdr);
				read_kernel_patchlist(bin, size, kernel_hdr, params);

				/* Verify checksum */
				auto checksum = gen_hash(kernel_data_start, bin - kernel_data_start) & 0xffffffff;
				if (kernel_hdr.CheckSum != checksum)
					throw invalid_argument("Kernel checksum mismatch for kernel `" + kernel_name + "'");

				/* Construct kernel */
				if (kernel_name == name)
				{
					if (kernel)
					{
						throw invalid_argument(
								"The given binary contains multiple kernels "
								"with the requested name.");
					}

					kernel = make_shared<I915KernelImpl>(
							kernel_name,
							params,
							move(kernel_heap),
							move(dynamic_state_heap),
							move(surface_state_heap),
							build_log);
				}
			}
			break;

		default:
			throw invalid_argument("Unsupported device family");
		};
	}

	if (size != 0)
		throw invalid_argument("Remaining size in binary file is not zero");

	if (!kernel)
	{
		throw invalid_argument(
				"A kernel with the given name was not found in the given binary.");
	}

	return kernel;
}


I915UserptrBo::I915UserptrBo(I915RTEImpl& rte, size_t req_size)
	: rte(rte), allocated(true)
{
	auto page_size = rte.get_page_size();
	_size = ((req_size + page_size - 1) / page_size) * page_size;

	_ptr = aligned_alloc(page_size, _size);
	if (!_ptr)
		throw system_error(errno, generic_category(), "Failed to allocate memory");

	try
	{
		_handle = rte.gem_userptr(_ptr, _size);
	}
	catch (...)
	{
		free(_ptr);
		throw;
	}
}

I915UserptrBo::I915UserptrBo(I915RTEImpl& rte, void* _ptr, size_t _size)
	: rte(rte), allocated(false), _ptr(_ptr), _size(_size)
{
	auto page_size = rte.get_page_size();
	if (_size % page_size != 0 || (uintptr_t) _ptr % page_size != 0)
		throw invalid_argument("size and ptr must be aligned to the system's page size");

	_handle = rte.gem_userptr(_ptr, _size);
}

I915UserptrBo::~I915UserptrBo()
{
	rte.gem_close(_handle);

	if (allocated)
		free(_ptr);
}

void* I915UserptrBo::ptr() const
{
	return _ptr;
}

size_t I915UserptrBo::size() const
{
	return _size;
}

uint32_t I915UserptrBo::handle() const
{
	return _handle;
}


/************************** Actual OpenCL Runtime class ***********************/
I915RTEImpl::I915RTEImpl(const char* device)
	: device_path(device)
{
	/* Ensure that the page size is 4kib */
	page_size = OCL::get_page_size();
	if (page_size != 4096)
		throw runtime_error("The system's page size is not 4096.");

	/* Try to open DRM device */
	fd = open(device_path.c_str(), O_RDWR);
	if (fd < 0)
		throw system_error(errno, generic_category(), "Failed to open DRM device");

	try
	{
		/* Query driver version */
		memset(driver_name, 0, ARRAY_SIZE(driver_name));
		driver_version = get_drm_version(fd, driver_name, ARRAY_SIZE(driver_name));

		if (strcmp(driver_name, "i915") != 0)
			throw runtime_error(string("Unsupported DRM driver:") + driver_name);

		/* Query device info using MESA's functions */
		if (!intel_get_device_info_from_fd(fd, &dev_info))
			throw runtime_error("Failed to query drm device info");

		/* Query chipset id and revision */
		dev_id = i915_getparam(fd, I915_PARAM_CHIPSET_ID);
		dev_revision = i915_getparam(fd, I915_PARAM_REVISION);

		/* Only support Gen9 for now */
		if (dev_info.ver != 9)
			throw runtime_error("Currently only Gen9 devices are supported");

		/* We only support cpu-cache coherent write-combining mmap topologies by
		 * now (i.e. newer integrated graphics). */
		if (!gem_supports_wc_mmap(fd))
			throw runtime_error("Coherent wc mmap is not supported by GPU");

		/* Check if we have execbuf2 */
		if (i915_getparam(fd, I915_PARAM_HAS_EXECBUF2) != 1)
			throw runtime_error("Device does not support EXECBUF2");

		/* Check if other required capabilities are supported */
		if (i915_getparam(fd, I915_PARAM_HAS_EXEC_NO_RELOC) != 1)
			throw runtime_error("Devices does not suport EXEC_NO_RELOC");

		try
		{
			has_userptr_probe = false;
			has_userptr_probe = i915_getparam(fd, I915_PARAM_HAS_USERPTR_PROBE) > 0 ? true : false;
		}
		catch (system_error& e)
		{
			if (e.code().value() != EINVAL)
				throw;
		}

		/* Create context */
		vm_id = gem_vm_create(fd);

		try
		{
			ctx_id = gem_context_create(fd);

			try
			{
				gem_context_set_vm(fd, ctx_id, vm_id);
			}
			catch (...)
			{
				gem_context_destroy(fd, ctx_id);
				throw;
			}
		}
		catch (...)
		{
			gem_vm_destroy(fd, vm_id);
			throw;
		}
	}
	catch (...)
	{
		close(fd);
		throw;
	}
}

I915RTEImpl::~I915RTEImpl()
{
	gem_context_destroy(fd, ctx_id);
	gem_vm_destroy(fd, vm_id);
	close(fd);
}

shared_ptr<Kernel> I915RTEImpl::compile_kernel(
		const char* src, const char* name, const char* options)
{
	IGCInterface igc(dev_id, dev_revision, dev_info);
	auto kernel_bin = igc.build(src, options);
	auto build_log = igc.get_build_log();

	if (!kernel_bin)
		throw runtime_error("Failed to compile kernel:\n" + build_log);

	/* Read IGC kernel binary */
	return I915KernelImpl::read_kernel(kernel_bin->get_bin(), kernel_bin->bin_size, name, build_log);
}

unique_ptr<PreparedKernel> I915RTEImpl::prepare_kernel(std::shared_ptr<Kernel> _kernel)
{
	auto kernel = dynamic_pointer_cast<I915KernelImpl>(_kernel);
	if (!kernel)
		throw invalid_argument("Given Kernel must be an I915Kernel");

	auto pkernel = make_unique<I915PreparedKernelImpl>(*this, kernel);
	return pkernel;
}

size_t I915RTEImpl::get_page_size()
{
	return page_size;
}

size_t I915RTEImpl::align_size_to_page(size_t size)
{
	return ((size + page_size - 1) / page_size) * page_size;
}

uint32_t I915RTEImpl::gem_userptr(void* ptr, size_t size)
{
	return OCL::gem_userptr(fd, ptr, size, has_userptr_probe);
}

void I915RTEImpl::gem_close(uint32_t handle)
{
	OCL::gem_close(fd, handle);
}

}
