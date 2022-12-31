/** Wrapper around IGC to simplify use for our usecase
 *
 * Inspired by IGC usage in intel-compute-runtime* (mainly by its offline
 * compiler, i.e. ocloc)
 *
 * References:
 *   * intel-compute-runtime: https://github.com/intel/compute-runtime
 */
#ifndef __IGC_INTERFACE_H
#define __IGC_INTERFACE_H

#include <cif/common/library_api.h>
#include <cif/common/cif_main.h>
#include <ocl_igc_interface/fcl_ocl_device_ctx.h>
#include <ocl_igc_interface/igc_ocl_device_ctx.h>

class IGCInterface
{
protected:
	struct DLLibrary final
	{
		void* handle = nullptr;

		DLLibrary(const char* name);

		DLLibrary(const DLLibrary&) = delete;
		DLLibrary& operator=(const DLLibrary&) = delete;

		~DLLibrary();
	};

	DLLibrary fcl_library;
	DLLibrary igc_library;

	CIF::RAII::UPtr_t<CIF::CIFMain> fcl_main = nullptr;
	CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fcl_device_ctx = nullptr;
	IGC::CodeType::CodeType_t preferred_ir;

	CIF::RAII::UPtr_t<CIF::CIFMain> igc_main = nullptr;
	CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igc_device_ctx = nullptr;

public:
	IGCInterface();

	IGCInterface(const IGCInterface&) = delete;
	IGCInterface& operator=(const IGCInterface&) = delete;

	virtual ~IGCInterface();
};

#endif /* __IGC_INTERFACE_H */
