#ifndef __HW_CMDS_H
#define __HW_CMDS_H

#include <optional>
#include "bitfield.h"

namespace HWCmds
{
	struct Command
	{
		virtual ~Command() = 0;
		virtual size_t write(char* dst) const = 0;
	};

	namespace Gen9
	{
		struct MiNoop : Command
		{
			size_t write(char* dst) const override;
		};

		struct MiBatchBufferEnd : Command
		{
			size_t write(char* dst) const override;
		};

		struct PipeControl : Command
		{
			bool cs_stall = false;
			bool generic_media_state_clear = false;
			bool flush_caches = false;
			bool invalidate_caches = false;

			size_t write(char* dst) const override;
		};

		struct _3DStateCCStatePointers : Command
		{
			bool color_calc_state_pointer_valid = false;

			size_t write(char* dst) const override;
		};

		/* Currently always disables the binding table pool allocator */
		struct _3DStateBindingTablePoolAlloc : Command
		{
			size_t write(char* dst) const override;
		};

		struct MiRSControl : Command
		{
			bool rs_enabled = false;

			size_t write(char* dst) const override;
		};

		struct PipelineSelect : Command
		{
			std::optional<bool> force_media_awake = std::nullopt;
			std::optional<bool> dop_clock_enable = std::nullopt;

			enum PipelineSelection {
				PIPELINE_3D = 0,
				PIPELINE_MEDIA = 1,
				PIPELINE_GPGPU = 2
			};
			std::optional<PipelineSelection> pipeline_selection = std::nullopt;

			size_t write(char* dst) const override;
		};

		struct StateBaseAddress : Command
		{
			/* The lower 12 bits of the base address will be ignored (take floor
			 * of each address w.r.t. 4k page size */
			std::optional<uint64_t> general_state_base_address = std::nullopt;
			uint8_t general_state_mocs = 0;
			uint8_t stateless_data_port_access_mocs = 0;

			std::optional<uint64_t> surface_state_base_address = std::nullopt;
			uint8_t surface_state_mocs = 0;

			std::optional<uint64_t> dynamic_state_base_address = std::nullopt;
			uint8_t dynamic_state_mocs = 0;

			std::optional<uint64_t> indirect_object_base_address = std::nullopt;
			uint8_t indirect_object_mocs = 0;

			std::optional<uint64_t> instruction_base_address = std::nullopt;
			uint8_t instruction_mocs = 0;

			/* Sizes are in 4k pages */
			std::optional<uint32_t> general_state_buffer_size = std::nullopt;
			std::optional<uint32_t> dynamic_state_buffer_size = std::nullopt;
			std::optional<uint32_t> indirect_object_buffer_size = std::nullopt;
			std::optional<uint32_t> instruction_buffer_size = std::nullopt;

			std::optional<uint64_t> bindless_surface_state_base_address = std::nullopt;
			uint8_t bindless_surface_state_mocs = 0;

			std::optional<uint32_t> bindless_surface_state_size = std::nullopt;

			size_t write(char* dst) const override;
		};

		struct MediaVFEState : Command
		{
			uint64_t scratch_space_base_pointer = 0;
			uint8_t stack_size = 0;
			uint8_t per_thread_scratch_space = 0;

			/* NOTE: \in [1,128], must not be 0 */
			uint8_t number_of_urb_entries = 0;
			bool reset_gateway_timer = 0;

			uint16_t urb_allocation_size = 0;
			uint16_t curbe_allocation_size = 0;

			size_t write(char* dst) const override;
		};

		struct MediaInterfaceDescriptorLoad : Command
		{
			uint32_t interface_descriptor_total_length = 0;
			uint32_t interface_descriptor_data_start_address = 0;

			size_t write(char* dst) const override;
		};

		struct MediaCurbeLoad : Command
		{
			uint32_t curbe_total_data_length = 0;
			uint32_t curbe_data_start_address = 0;

			size_t write(char* dst) const override;
		};

		struct MediaStateFlush : Command
		{
			bool flush_to_go = false;
			std::optional<uint8_t> watermark_interface_descriptor = std::nullopt;

			size_t write(char* dst) const override;
		};

		struct GPGPUWalker : Command
		{
			uint8_t interface_descriptor_offset = 0;

			uint16_t indirect_data_length = 0;
			uint32_t indirect_data_start_address = 0;

			enum
			{
				SIMD8 = 0,
				SIMD16 = 1,
				SIMD32 = 2
			}
			simd_size;

			uint8_t thread_width_counter_maximum = 0;
			uint8_t thread_height_counter_maximum = 0;
			uint8_t thread_depth_counter_maximum = 0;

			uint32_t thread_group_id_starting_x = 0;
			uint32_t thread_group_id_starting_y = 0;
			uint32_t thread_group_id_starting_resume_z = 0;

			uint32_t thread_group_id_x_dimension = 0;
			uint32_t thread_group_id_y_dimension = 0;
			uint32_t thread_group_id_z_dimension = 0;

			uint32_t right_execution_mask = 0;
			uint32_t bottom_execution_mask = 0;

			size_t write(char* dst) const override;
		};


		/* Structures */
		struct __attribute__((packed)) InterfaceDescriptorData 
		{
			uint32_t raw[8];
		};

		struct InterfaceDescriptorDataAccessor
		{
			union
			{
				InterfaceDescriptorData data;

				BitField<uint32_t, 0, 6,31> kernel_start_pointer;

				BitField<uint32_t, 1, 0, 15> kernel_start_pointer_high;

				BitField<uint32_t, 2, 19,19> denorm_mode;
				BitField<uint32_t, 2, 18,18> single_program_flow;
				BitField<uint32_t, 2, 17,17> thread_priority;
				BitField<uint32_t, 2, 16, 16> floating_point_mode;
				BitField<uint32_t, 2, 13, 13> illegal_opcode_exception_enable;
				BitField<uint32_t, 2, 11, 11> mask_stack_exception_enable;
				BitField<uint32_t, 2, 7, 7> software_exception_enable;

				BitField<uint32_t, 3, 5, 31> sampler_state_pointer;
				BitField<uint32_t, 3, 2, 4> sampler_count;

				BitField<uint32_t, 4, 5, 15> binding_table_pointer;
				BitField<uint32_t, 4, 0, 4> binding_table_entry_count;

				BitField<uint32_t, 5, 16, 31> constant_indirect_urb_entry_read_length;
				BitField<uint32_t, 5, 0, 15> constant_urb_entry_read_offset;

				BitField<uint32_t, 6, 22, 23> rounding_mode;
				BitField<uint32_t, 6, 21, 21> barrier_enable;
				BitField<uint32_t, 6, 16, 20> shared_local_memory_size;
				BitField<uint32_t, 6, 15, 15> global_barrier_enable;
				BitField<uint32_t, 6, 0, 9> number_of_threads_in_gpgpu_thread_group;

				BitField<uint32_t, 7, 0, 7> cross_thread_constant_data_read_length;
			}
			common;

			InterfaceDescriptorDataAccessor();

			/* Helper functions */
			inline uint64_t get_kernel_start_pointer()
			{
				return (uint64_t) common.kernel_start_pointer.get() << 6 |
					(uint64_t) common.kernel_start_pointer_high.get() << 32;
			}

			inline void set_kernel_start_pointer(uint64_t p)
			{
				common.kernel_start_pointer.set((p & 0xffffffff) >> 6);
				common.kernel_start_pointer_high.set((p >> 32) & 0xffff);
			}

			void set_shared_local_memory_size_bytes(unsigned size);
		};
		static_assert(sizeof(InterfaceDescriptorDataAccessor) == 32);
	};

	template<typename _Iterator>
	size_t write_cmds(char* buf, _Iterator first, _Iterator last)
	{
		size_t size = 0;

		for (_Iterator c = first; c != last; c++)
		{
			auto res = (*c)->write(buf);
			buf += res;
			size += res;
		}

		return size;
	}
};

#endif /* __HW_CMDS_H */
