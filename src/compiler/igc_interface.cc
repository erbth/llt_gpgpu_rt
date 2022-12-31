/** Wrapper around IGC to simplify use for our usecase
 *
 * Inspired by and adapted from IGC usage in intel-compute-runtime* (mainly
 * by/from its offline compiler, i.e. ocloc)
 *
 * References:
 *   * intel-compute-runtime: https://github.com/intel/compute-runtime
 */
#include <vector>
#include <string>
#include <stdexcept>

extern "C" {
#include <dlfcn.h>
}

#include <igc.opencl.h>

#include "igc_interface.h"

using namespace std;


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
IGCInterface::IGCInterface()
	: fcl_library(FCL_LIBRARY_NAME), igc_library(IGC_LIBRARY_NAME)
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

	/* intel-compute-runtime sets platform-specific information here if
	 * GetUnderlyingVersion() returns > 4u. The latter does not in our case; but
	 * it is not clear if that platform specific information is optional, hence
	 * ensure that GetUnderlyingVersion() is <= 4u for now. */
	if (fcl_device_ctx->GetUnderlyingVersion() > 4u)
		throw runtime_error("FCL is too new");


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
	IGC::GtSysInfoHelper::PopulateInterfaceWith(*gt_system_info, hwinfo.gtSystemInfo);

	/* Set features */
	ftr_wa->SetFtrDesktop(hw_info.featureTable.flags.ftrDesktop);
	ftr_wa->SetFtrChannelSwizzlingXOREnabled(hw_info.featureTable.flags.ftrChannelSwizzlingXOREnabled);
	ftr_wa->SetFtrIVBMOM1Platform(hw_info.featureTable.flags.ftrIVBMOM1Platform);
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
