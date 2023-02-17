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

#include <string>
#include <memory>

#include <cif/common/library_api.h>
#include <cif/common/cif_main.h>
#include <ocl_igc_interface/fcl_ocl_device_ctx.h>
#include <ocl_igc_interface/igc_ocl_device_ctx.h>

#include "hw_info.h"

class IGCInterface
{
public:
	class IntermediateRepresentation final
	{
	protected:
		CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> data;

		const char* data_ptr;
		size_t data_size;

	public:
		const IGC::CodeType::CodeType_t code_type;

		IntermediateRepresentation(
				CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL>&& data,
				IGC::CodeType::CodeType_t code_type);

		IntermediateRepresentation(const IntermediateRepresentation&) = delete;
		IntermediateRepresentation& operator=(const IntermediateRepresentation&) = delete;

		IGC::OclTranslationOutputTagOCL* get_output();

		size_t get_data_size() const;
		const char* get_data_ptr() const;
	};

	class Binary final
	{
	protected:
		char* bin;
		char* debug;

	public:
		const size_t bin_size;
		const size_t debug_size;

		Binary(const char* bin, size_t bin_size,
				const char* debug, size_t debug_size);

		Binary(const Binary&) = delete;
		Binary& operator=(const Binary&) = delete;

		~Binary();

		const char* get_bin() const;
		const char* get_debug() const;
	};

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

	NEO::HardwareInfo hw_info;

	CIF::RAII::UPtr_t<CIF::CIFMain> fcl_main = nullptr;
	CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fcl_device_ctx = nullptr;
	IGC::CodeType::CodeType_t preferred_ir;

	CIF::RAII::UPtr_t<CIF::CIFMain> igc_main = nullptr;
	CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igc_device_ctx = nullptr;

	std::vector<std::string> get_supported_extensions();
	std::string get_internal_options();
	std::unique_ptr<IntermediateRepresentation> build_ir(
			const std::string& src,
			CIF::Builtins::BufferLatest* options,
			CIF::Builtins::BufferLatest* internal_options);

	std::string build_log;

public:
	IGCInterface(const NEO::HardwareInfo& hw_info);

	IGCInterface(const IGCInterface&) = delete;
	IGCInterface& operator=(const IGCInterface&) = delete;

	virtual ~IGCInterface();

	std::unique_ptr<Binary> build(const std::string& src, const std::string& options);
	std::string get_build_log() const;
};

#endif /* __IGC_INTERFACE_H */
