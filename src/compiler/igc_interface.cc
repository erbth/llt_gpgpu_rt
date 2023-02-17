/** Wrapper around IGC to simplify use for our usecase
 *
 * Inspired by and adapted from IGC usage in intel-compute-runtime* (mainly
 * by/from its offline compiler, i.e. ocloc)
 *
 * References:
 *   * intel-compute-runtime: https://github.com/intel/compute-runtime
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include <new>
#include <stdexcept>

extern "C" {
#include <dlfcn.h>
}

#include "igc_interface.h"

#include <igc.opencl.h>
#include <ocl_igc_interface/platform_helper.h>

using namespace std;


IGCInterface::IntermediateRepresentation::IntermediateRepresentation(
		CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL>&& data,
		IGC::CodeType::CodeType_t code_type)
	:
		data(move(data)), code_type(code_type)
{
	data_ptr = nullptr;
	data_size = 0;

	if (this->data && this->data->GetOutput() &&
			this->data->GetOutput()->GetSizeRaw() > 0 &&
			this->data->GetOutput()->GetMemory<char>())
	{
		data_ptr = this->data->GetOutput()->GetMemory<char>();
		data_size = this->data->GetOutput()->GetSize<char>();
	}
}

IGC::OclTranslationOutputTagOCL* IGCInterface::IntermediateRepresentation::get_output()
{
	return data.get();
}

size_t IGCInterface::IntermediateRepresentation::get_data_size() const
{
	return data_size;
}

const char* IGCInterface::IntermediateRepresentation::get_data_ptr() const
{
	return data_ptr;
}


IGCInterface::Binary::Binary(const char* _bin, size_t bin_size,
		const char* _debug, size_t debug_size)
	:
		bin_size(bin_size), debug_size(debug_size)
{
	bin = (char*) malloc(bin_size);
	if (!bin)
		throw bad_alloc();

	debug = (char*) malloc(debug_size);
	if (!debug)
	{
		free(bin);
		throw bad_alloc();
	}

	memcpy(bin, _bin, bin_size);
	memcpy(debug, _debug, debug_size);
}

IGCInterface::Binary::~Binary()
{
	free(debug);
	free(bin);
}

const char* IGCInterface::Binary::get_bin() const
{
	return bin;
}

const char* IGCInterface::Binary::get_debug() const
{
	return debug;
}


IGCInterface::DLLibrary::DLLibrary(const char* name)
{
	handle = dlopen(name, RTLD_NOW);
	if (!handle)
		throw runtime_error(string("Failed to load dynamic library \"") + name + "\"");
}

IGCInterface::DLLibrary::~DLLibrary()
{
	if (dlclose(handle) != 0)
		terminate();
}

/* Adapted from intel-compute-runtime -
 * shared/offline_compiler/source/offline_compiler.cpp ::initialize */
IGCInterface::IGCInterface(const NEO::HardwareInfo& hw_info)
	:
		fcl_library(FCL_LIBRARY_NAME), igc_library(IGC_LIBRARY_NAME),
		hw_info(hw_info)
{
	/* Initialize FCL */
	auto fcl_create_main = (CIF::CreateCIFMainFunc_t) dlsym(
			fcl_library.handle, CIF::CreateCIFMainFuncName);

	if (!fcl_create_main)
		throw runtime_error("Failed to find FCL CreateCIFMainFunc");

	fcl_main = CIF::RAII::UPtr_t<CIF::CIFMain>(fcl_create_main());
	if (!fcl_main)
		throw runtime_error("Failed to create FCL CIFMain");

	if (!fcl_main->IsCompatible<IGC::FclOclDeviceCtx>())
		throw runtime_error("Incompatible interface in FCL");

	fcl_device_ctx = fcl_main->CreateInterface<IGC::FclOclDeviceCtxTagOCL>();
	if (!fcl_device_ctx)
		throw runtime_error("Failed to create FCL device ctx");

	/* Set OpenCL version
	 * NOTE: Stick to OpenCL 1.2 for now s.t. the runtime does not have to
	 * support more complex OCL 2/3 features. And all used devices should
	 * support at least OCL 1.2.
	 * */
	fcl_device_ctx->SetOclApiVersion(120);
	preferred_ir = fcl_device_ctx->GetPreferredIntermediateRepresentation();

	if (fcl_device_ctx->GetUnderlyingVersion() > 4u)
	{
#if 0
		auto fcl_platform = fcl_device_ctx->GetPlatformHandle();
		if (!fcl_platform)
			throw runtime_error("Failed to set platform for FCL");

		IGC::PlatformHelper::PopulateInterfaceWith(
				fcl_platform, (const HardwareInfo&) hw_info.platform);
#else
		throw runtime_error("FCL too new");
#endif
	}


	/* Initialize IGC */
	auto igc_create_main = (CIF::CreateCIFMainFunc_t) dlsym(
			igc_library.handle, CIF::CreateCIFMainFuncName);

	if (!igc_create_main)
		throw runtime_error("Failed to find IGC CreateCIFMainFunc");

	igc_main = CIF::RAII::UPtr_t<CIF::CIFMain>(igc_create_main());
	if (!igc_main)
		throw runtime_error("Failed to create IGC CIFMain");

	/* Check IGC compatibility */
	vector<CIF::InterfaceId_t> interfaces_to_ignore = { IGC::OclGenBinaryBase::GetInterfaceId() };
	if (!igc_main->IsCompatible<IGC::IgcOclDeviceCtx>(&interfaces_to_ignore))
		throw runtime_error("Incompatible interface in IGC");

	CIF::Version_t ver_min = 0, ver_max = 0;
	if (!igc_main->FindSupportedVersions<IGC::IgcOclDeviceCtx>(
				IGC::OclGenBinaryBase::GetInterfaceId(), ver_min, ver_max))
	{
		throw runtime_error("IGC misses the Patchtoken interface");
	}

	igc_device_ctx = igc_main->CreateInterface<IGC::IgcOclDeviceCtxTagOCL>();
	if (!igc_device_ctx)
		throw runtime_error("Failed to create IGC device ctx");


	/* Configure target device */
	igc_device_ctx->SetProfilingTimerResolution(static_cast<float>(
				hw_info.capabilityTable.defaultProfilingTimerResolution));

	auto platform = igc_device_ctx->GetPlatformHandle();
	auto gt_system_info = igc_device_ctx->GetGTSystemInfoHandle();
	auto ftr_wa = igc_device_ctx->GetIgcFeaturesAndWorkaroundsHandle();

	if (!platform || !gt_system_info || !ftr_wa)
		throw runtime_error("IGC: Failed to get handle for device configuration");

	IGC::PlatformHelper::PopulateInterfaceWith(*platform, hw_info.platform);
	IGC::GtSysInfoHelper::PopulateInterfaceWith(*gt_system_info, hw_info.gtSystemInfo);

	/* Set features */
	ftr_wa->SetFtrDesktop(hw_info.featureTable.flags.ftrDesktop);
	ftr_wa->SetFtrChannelSwizzlingXOREnabled(hw_info.featureTable.flags.ftrChannelSwizzlingXOREnabled);
	ftr_wa->SetFtrIVBM0M1Platform(hw_info.featureTable.flags.ftrIVBM0M1Platform);
	ftr_wa->SetFtrSGTPVSKUStrapPresent(hw_info.featureTable.flags.ftrSGTPVSKUStrapPresent);
	ftr_wa->SetFtr5Slice(hw_info.featureTable.flags.ftr5Slice);

	/* Requires BDW or later (i.e. Gen8 or later)
	 * TODO: add check for that (for older generations simply leave the entire
	 * call out). */
	ftr_wa->SetFtrGpGpuMidThreadLevelPreempt(hw_info.featureTable.flags.ftrGpGpuMidThreadLevelPreempt);

	ftr_wa->SetFtrIoMmuPageFaulting(hw_info.featureTable.flags.ftrIoMmuPageFaulting);
	ftr_wa->SetFtrWddm2Svm(hw_info.featureTable.flags.ftrWddm2Svm);
	ftr_wa->SetFtrPooledEuEnabled(hw_info.featureTable.flags.ftrPooledEuEnabled);

	ftr_wa->SetFtrResourceStreamer(hw_info.featureTable.flags.ftrResourceStreamer);
}

IGCInterface::~IGCInterface()
{
}


vector<string> IGCInterface::get_supported_extensions()
{
	vector<string> exts;

	/* Adapted from intel-compute-runtime,
	 * shared/source/compiler_interface/oclc_extensions.cpp */
	exts.push_back("cl_khr_byte_addressable_store");
	exts.push_back("cl_khr_fp16");
	exts.push_back("cl_khr_global_int32_base_atomics");
	exts.push_back("cl_khr_global_int32_extended_atomics");
	exts.push_back("cl_khr_icd");
	exts.push_back("cl_khr_local_int32_base_atomics");
	exts.push_back("cl_khr_local_int32_extended_atomics");
	exts.push_back("cl_intel_command_queue_families");
	exts.push_back("cl_intel_subgroups");
	exts.push_back("cl_intel_required_subgroup_size");
	exts.push_back("cl_intel_subgroups_short");
	exts.push_back("cl_khr_spir");
	exts.push_back("cl_intel_accelerator");
	exts.push_back("cl_intel_driver_diagnostics");
	exts.push_back("cl_khr_priority_hints");
	exts.push_back("cl_khr_throttle_hints");
	exts.push_back("cl_khr_create_command_queue");
	exts.push_back("cl_intel_subgroups_char");
	exts.push_back("cl_intel_subgroups_long");
	exts.push_back("cl_khr_il_program");
	exts.push_back("cl_intel_mem_force_host_memory");
	exts.push_back("cl_khr_subgroup_extended_types");
	exts.push_back("cl_khr_subgroup_non_uniform_vote");
	exts.push_back("cl_khr_subgroup_ballot");
	exts.push_back("cl_khr_subgroup_non_uniform_arithmetic");
	exts.push_back("cl_khr_subgroup_shuffle");
	exts.push_back("cl_khr_subgroup_shuffle_relative");
	exts.push_back("cl_khr_subgroup_clustered_reduce");
	exts.push_back("cl_intel_device_attribute_query");
	exts.push_back("cl_khr_suggested_local_work_size");

	return exts;
}

string IGCInterface::get_internal_options()
{
	string options;

	if (hw_info.capabilityTable.clVersionSupport >= 30)
		options += "-ocl-version=300 ";
	else if (hw_info.capabilityTable.clVersionSupport >= 21)
		options += "-ocl-version=210 ";
	else
		options += "-ocl-version=120 ";

	/* Add supported extensions */
	options += "-cl-ext=-all";

	for (auto& ext : get_supported_extensions())
		options += ",+" + ext;

	options += " ";

	/* E.g. GLK does not support independent subgroup forward progress; not sure
	 * if that is important; just tell the compiler to be on the safe side */
	options += "-cl-no-subgroup-ifp ";

	return options;
}

/* Adapted from intel-compute-runtime -
 * shared/offline_compiler/source/offline_compiler.cpp ::buildIrBinary and functions
 * called by it */
unique_ptr<IGCInterface::IntermediateRepresentation> IGCInterface::build_ir(
		const string& src,
		CIF::Builtins::BufferLatest* options,
		CIF::Builtins::BufferLatest* internal_options)
{
	auto err = CIF::Builtins::CreateConstBuffer(fcl_main.get(), nullptr, 0);
	auto translation_ctx = fcl_device_ctx->CreateTranslationCtx(
			IGC::CodeType::oclC,
			preferred_ir,
			err.get());

	if (err->GetMemory<char>())
	{
		throw runtime_error("Failed to create FCL translation ctx: " +
				string(err->GetMemory<char>(), err->GetSize<char>()));
	}

	if (!translation_ctx.get())
		throw runtime_error("Failed to create FCL translation ctx");

	/* Create buffers */
	/* Not sure why +1 is required here, Intel code has it and without it the
	 * last character is missing in the .cl file dumped with
	 * IGC_ShaderDumpEnable=1 and if it is relevant compilitation fails (e.g. if
	 * it is the closing } of a kernel function). It is not required for e.g.
	 * the options, though (when present it introduces a NUL byte in the dumped
	 * options file). I guess this comes from wrong length calculation inside
	 * IGC, however the extra NUL should not cause OOB reads (as it is backed by
	 * real memory from the string); and valgrind does not show any errors from
	 * related code. In the .cl file dump there is always a NUL at the end, even
	 * without this +1 maybe this backs the theory of incorrect length
	 * calculation. */
	auto src_buf = CIF::Builtins::CreateConstBuffer(fcl_main.get(),
			src.c_str(), src.size() + 1);

	if (!src_buf)
		throw runtime_error("Failed to allocate buffer for source code");

	auto output = translation_ctx->Translate(
			src_buf.get(),
			options,
			internal_options,
			nullptr,
			0);

	if (
			output == nullptr ||
			output->GetBuildLog() == nullptr ||
			output->GetOutput() == nullptr)
	{
		throw runtime_error("Failed to translate source code to IR");
	}

	if (output->GetBuildLog()->GetSizeRaw() > 0 &&
			output->GetBuildLog()->GetMemory<char>())
	{
		build_log += string(output->GetBuildLog()->GetMemory<char>(),
				output->GetBuildLog()->GetSize<char>());
	}

	if (!output->Successful())
		return nullptr;

	return make_unique<IntermediateRepresentation>(move(output), preferred_ir);
}

/* Adapted from intel-compute-runtime -
 * shared/offline_compiler/source/offline_compiler.cpp ::build and functions
 * called by it */
unique_ptr<IGCInterface::Binary> IGCInterface::build(
		const string& src, const string& options)
{
	build_log.clear();

	auto options_buf = CIF::Builtins::CreateConstBuffer(fcl_main.get(),
			options.c_str(), options.size());

	if (!options_buf)
		throw runtime_error("Failed to create buffer for options");

	string internal_options = get_internal_options();
	auto internal_options_buf = CIF::Builtins::CreateConstBuffer(fcl_main.get(),
			internal_options.c_str(), internal_options.size());

	if (!internal_options_buf)
		throw runtime_error("Failed to create buffer for options");

	/* Build intermediate representation */
	auto ir = build_ir(src, options_buf.get(), internal_options_buf.get());
	if (!ir)
		return nullptr;

	/* Translate to machine code */
	auto translation_ctx = igc_device_ctx->CreateTranslationCtx(
			ir->code_type, IGC::CodeType::oclGenBin);

	if (!translation_ctx)
		throw runtime_error("Failed to create IGC translation ctx");

	auto output = translation_ctx->Translate(
			ir->get_output()->GetOutput(),
			options_buf.get(),
			internal_options_buf.get(), 
			nullptr,
			0);

	if (output == nullptr ||
			output->GetBuildLog() == nullptr ||
			output->GetOutput() == nullptr)
	{
		throw runtime_error("Failed to translate IR to binary");
	}

	if (output->GetBuildLog()->GetSizeRaw() > 0 &&
			output->GetBuildLog()->GetMemory<char>())
	{
		build_log += string(output->GetBuildLog()->GetMemory<char>(),
				output->GetBuildLog()->GetSize<char>());
	}

	const char* bin_data = nullptr;
	size_t bin_data_size = 0;
	if (output->GetOutput()->GetSizeRaw() > 0 && output->GetOutput()->GetMemory<char>())
	{
		bin_data = output->GetOutput()->GetMemory<char>();
		bin_data_size = output->GetOutput()->GetSize<char>();
	}

	const char* debug_data = nullptr;
	size_t debug_data_size = 0;
	if (output->GetDebugData() && output->GetDebugData()->GetSizeRaw() > 0 &&
			output->GetDebugData()->GetMemory<char>())
	{
		debug_data = output->GetDebugData()->GetMemory<char>();
		debug_data_size = output->GetDebugData()->GetSize<char>();
	}

	return make_unique<Binary>(
			bin_data, bin_data_size,
			debug_data, debug_data_size);
}

string IGCInterface::get_build_log() const
{
	return build_log;
}
