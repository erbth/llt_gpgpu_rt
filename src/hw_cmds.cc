#include <cstring>
#include <stdexcept>
#include "ensure_types.h"
#include "hw_cmds.h"

using namespace std;

#define MI_NOOP (0x00ULL)
#define MI_BATCH_BUFFER_END (0x0AULL << 23)

namespace HWCmds
{

Command::~Command()
{
}

namespace Gen9
{
	size_t MiNoop::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;
		*ptr = MI_NOOP;
		return 4;
	}

	size_t MiBatchBufferEnd::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;
		*ptr = MI_BATCH_BUFFER_END;
		return 4;
	}

	size_t PipeControl::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x3 << 27 | 0x2 << 24 | 4;

		uint32_t dw1 = 0;
		if (cs_stall)
			dw1 |= 1 << 20;

		if (generic_media_state_clear)
			dw1 |= 1 << 16;

		/* Flush render target cache, DC, and Depth cache */
		if (flush_caches)
			dw1 |= 1 << 12 | 1 << 5 | 1 << 0;

		/* Invalidate instruction cache, texture cache, constant cache, and
		 * state cache */
		if (invalidate_caches)
			dw1 |= 1 << 11 | 1 << 10 | 1 << 3 | 1 << 2;

		ptr[1] = dw1;

		ptr[2] = 0;
		ptr[3] = 0;
		ptr[4] = 0;
		ptr[5] = 0;

		return 6 * 4;
	}

	size_t _3DStateCCStatePointers::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x3 << 27 | 0xe << 16;

		uint32_t dw1 = 0;
		if (color_calc_state_pointer_valid)
			dw1 |= 1 << 0;

		ptr[1] = dw1;
		return 2 * 4;
	}

	size_t _3DStateBindingTablePoolAlloc::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x3 << 27 | 0x1 << 24 | 0x19 << 16 | 0x2;
		ptr[1] = 0;
		ptr[2] = 0;
		ptr[3] = 0;

		return 4 * 4;
	}

	size_t MiRSControl::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		uint32_t dw0 = 0x0 << 29 | 0x6 << 23;

		if (rs_enabled)
			dw0 |= 1 << 0;

		ptr[0] = dw0;
		return 4;
	}

	size_t PipelineSelect::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		uint32_t dw0 = 0x3 << 29 | 0x1 << 27 | 0x1 << 24 | 0x4 << 16;

		if (force_media_awake)
		{
			dw0 |= 1 << 13;
			if (*force_media_awake)
				dw0 |= 1 << 5;
		}

		if (dop_clock_enable)
		{
			dw0 |= 1 << 12;
			if (*dop_clock_enable)
				dw0 |= 1 << 4;
		}

		if (pipeline_selection)
		{
			dw0 |= 0x3 << 8;
			dw0 |= *pipeline_selection & 0x3;
		}

		ptr[0] = dw0;
		return 4;
	}

	size_t StateBaseAddress::write(char* dst) const
	{
		/* Empose constraints on arguments */
		if (general_state_base_address || general_state_buffer_size)
		{
			if (!(general_state_base_address && general_state_buffer_size))
			{
				throw invalid_argument(
						"general_state_base_address and "
						"general_state_buffer_size must both be set or none.");
			}

			if ((*general_state_base_address + *general_state_buffer_size) >= 0x1000000000000ULL)
				throw invalid_argument("general state buffer end out of range");
		}

		auto ptr = (uint32_t*) dst;
		int cnt_dws = bindless_surface_state_size ? 19 : 18;

		ptr[0] = 0x3 << 29 | 0x0 << 27 | 0x1 << 24 | 0x1 << 16 | ((cnt_dws - 2) & 0xff);

		uint32_t dw1 = (general_state_mocs & 0x7f) << 4;
		uint32_t dw2 = 0;

		if (general_state_base_address)
		{
			uint64_t addr = *general_state_base_address;
			addr &= 0xfffffffff000ULL;
			if (addr & 0x800000000000ULL)
				addr |= 0xffff000000000000ULL;

			dw1 |= (addr & 0xfffff000ULL) | 1 << 0;
			dw2 = (addr >> 32) & 0xffffffffULL;
		}

		ptr[1] = dw1;
		ptr[2] = dw2;
		ptr[3] = (stateless_data_port_access_mocs & 0x7f) << 16;

		uint32_t dw4 = (surface_state_mocs & 0x7f) << 4;
		uint32_t dw5 = 0;

		if (surface_state_base_address)
		{
			uint64_t addr = *surface_state_base_address;
			addr &= 0xfffffffff000ULL;
			if (addr & 0x800000000000ULL)
				addr |= 0xffff000000000000ULL;

			dw4 |= (addr & 0xfffff000ULL) | 1 << 0;
			dw5 = (addr >> 32) & 0xffffffffULL;
		}

		ptr[4] = dw4;
		ptr[5] = dw5;

		uint32_t dw6 = (dynamic_state_mocs & 0x7f) << 4;
		uint32_t dw7 = 0;

		if (dynamic_state_base_address)
		{
			uint64_t addr = *dynamic_state_base_address;
			addr &= 0xfffffffff000ULL;
			if (addr & 0x800000000000ULL)
				addr |= 0xffff000000000000ULL;

			dw6 |= (addr & 0xfffff000ULL) | 1 << 0;
			dw7 = (addr >> 32) & 0xffffffffULL;
		}

		ptr[6] = dw6;
		ptr[7] = dw7;

		uint32_t dw8 = (indirect_object_mocs & 0x7f) << 4;
		uint32_t dw9 = 0;

		if (indirect_object_base_address)
		{
			uint64_t addr = *indirect_object_base_address;
			addr &= 0xfffffffff000ULL;
			if (addr & 0x800000000000ULL)
				addr |= 0xffff000000000000ULL;

			dw8 |= (addr & 0xfffff000ULL) | 1 << 0;
			dw9 = (addr >> 32) & 0xffffffffULL;
		}

		ptr[8] = dw8;
		ptr[9] = dw9;

		uint32_t dw10 = (instruction_mocs & 0x7f) << 4;
		uint32_t dw11 = 0;

		if (instruction_base_address)
		{
			uint64_t addr = *instruction_base_address;
			addr &= 0xfffffffff000ULL;
			if (addr & 0x800000000000ULL)
				addr |= 0xffff000000000000ULL;

			dw10 |= (addr & 0xfffff000ULL) | 1 << 0;
			dw11 = (addr >> 32) & 0xffffffffULL;
		}

		ptr[10] = dw10;
		ptr[11] = dw11;

		ptr[12] = 0;
		if (general_state_buffer_size)
			ptr[12] |= *general_state_buffer_size << 12 | 1 << 0;

		ptr[13] = 0;
		if (dynamic_state_buffer_size)
			ptr[13] |= *dynamic_state_buffer_size << 12 | 1 << 0;

		ptr[14] = 0;
		if (indirect_object_buffer_size)
			ptr[14] |= *indirect_object_buffer_size << 12 | 1 << 0;

		ptr[15] = 0;
		if (instruction_buffer_size)
			ptr[15] |= *instruction_buffer_size << 12 | 1 << 0;

		uint32_t dw16 = (bindless_surface_state_mocs & 0x7f) << 4;
		uint32_t dw17 = 0;

		if (bindless_surface_state_base_address)
		{
			uint64_t addr = *bindless_surface_state_base_address;
			addr &= 0xfffffffff000ULL;
			if (addr & 0x800000000000ULL)
				addr |= 0xffff000000000000ULL;

			dw16 |= (addr & 0xfffff000ULL) | 1 << 0;
			dw17 = (addr >> 32) & 0xffffffffULL;
		}

		ptr[16] = dw16;
		ptr[17] = dw17;

		if (bindless_surface_state_size)
			ptr[18] = *bindless_surface_state_size << 12;

		return cnt_dws * 4;
	}

	size_t MediaVFEState::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x2 << 27 | 0 << 24 | 0 << 16 | 0x7;

		ptr[1] = (scratch_space_base_pointer & 0xfffffc00ULL) |
			(stack_size & 0xf) << 4 | (per_thread_scratch_space & 0xf) << 0;

		ptr[2] = (scratch_space_base_pointer >> 32) & 0xffffULL;

		uint32_t dw3 = 0x7fff << 16 | (number_of_urb_entries & 0xff) << 8;
		if (reset_gateway_timer)
			dw3 |= 0x1 << 7;

		ptr[3] = dw3;

		ptr[4] = 0;
		ptr[5] = urb_allocation_size << 16 | curbe_allocation_size;

		ptr[6] = 0;
		ptr[7] = 0;
		ptr[8] = 0;

		return 9 * 4;
	}

	size_t MediaInterfaceDescriptorLoad::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x2 << 27 | 0 << 24 | 0x2 << 16 | 0x2;
		ptr[1] = 0;

		ptr[2] = interface_descriptor_total_length & 0x1ffff;
		ptr[3] = interface_descriptor_data_start_address;

		return 4 * 4;
	}

	size_t MediaCurbeLoad::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x2 << 27 | 0 << 24 | 0x1 << 16 | 0x2;
		ptr[1] = 0;
		ptr[2] = curbe_total_data_length & 0x1ffff;
		ptr[3] = curbe_data_start_address;

		return 4 * 4;
	}

	size_t MediaStateFlush::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x2 << 27 | 0 << 24 | 0x4 << 16 | 0;

		uint32_t dw1 = 0;
		if (flush_to_go)
			dw1 |= 1 << 7;

		if (watermark_interface_descriptor)
			dw1 |= 1 << 6 | (*watermark_interface_descriptor & 0x3f);

		ptr[1] = dw1;

		return 2 * 4;
	}

	size_t GPGPUWalker::write(char* dst) const
	{
		auto ptr = (uint32_t*) dst;

		ptr[0] = 0x3 << 29 | 0x2 << 27 | 0x1 << 24 | 0x5 << 16 | 0xd;
		ptr[1] = interface_descriptor_offset & 0x3f;
		ptr[2] = indirect_data_length;
		ptr[3] = indirect_data_start_address & ~0x3fULL;

		ptr[4] = (simd_size & 0x3UL) << 30 | (thread_depth_counter_maximum & 0x3f) << 16 |
			(thread_height_counter_maximum & 0x3f) << 8 | (thread_width_counter_maximum & 0x3f) << 0;

		ptr[5] = thread_group_id_starting_x;
		ptr[6] = 0;
		ptr[7] = thread_group_id_x_dimension;
		ptr[8] = thread_group_id_starting_y;
		ptr[9] = 0;
		ptr[10] = thread_group_id_y_dimension;
		ptr[11] = thread_group_id_starting_resume_z;
		ptr[12] = thread_group_id_z_dimension;

		ptr[13] = right_execution_mask;
		ptr[14] = bottom_execution_mask;

		return 15 * 4;
	}


	InterfaceDescriptorDataAccessor::InterfaceDescriptorDataAccessor()
	{
		memset(&(common.data), 0, sizeof(common.data));
	}

	void InterfaceDescriptorDataAccessor::set_shared_local_memory_size_bytes(unsigned size)
	{
		uint32_t v;

		if (size == 0)
		{
			v = 0;
		}
		else
		{
			v = 1;
			unsigned s = 1024;

			while (size > s)
			{
				v += 1;
				s *= 2;

				if (v > 7)
					throw invalid_argument("SLM size must be <= 64kiB");
			}
		}

		common.shared_local_memory_size.set(v);
	}
};

};
