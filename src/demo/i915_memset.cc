#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <llt_gpgpu_rt/i915_runtime.h>
#include "utils.h"
#include "i915_memset.clch"

using namespace std;


int main(int argc, char** argv)
{
	try
	{
		auto rte = OCL::create_i915_rte("/dev/dri/card0");
		auto kernel = rte->read_compiled_kernel(CompiledGPUProgramsI915::i915_memset(), "cl_memset");

		/* Allocate dummy memory */
		AlignedBuffer buf(rte->get_page_size(), 1024ULL * 1024ULL * 1024ULL);
		memset(buf.ptr(), 0, buf.size());

		/* Execute kernel */
		auto pkernel = rte->prepare_kernel(kernel);
		pkernel->add_argument((unsigned) buf.size() / 4);
		pkernel->add_argument(0x12345678U);
		pkernel->add_argument((void*) buf.ptr(), buf.size());
		pkernel->execute(OCL::NDRange(DIV_ROUND_UP(buf.size(), 4)), OCL::NDRange(256));

		/* Compare result */
		for (size_t i = 0; i < buf.size() / 4; i++)
		{
			auto val = ((const uint32_t*) buf.ptr())[i];
			if (val != 0x12345678U)
			{
				printf("Missmatch at address 0x%08x: 0x%08x\n", (int) i*4, (int) val);
				break;
			}
		}
	}
	catch (exception& e)
	{
		fprintf(stderr, "ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
