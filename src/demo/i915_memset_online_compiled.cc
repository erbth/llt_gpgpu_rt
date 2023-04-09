#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <llt_gpgpu_rt/i915_runtime.h>
#include "utils.h"

using namespace std;

const char* kernel_src = R"KERNELSRC(
void __kernel cl_memset(uint size, uint val, __global uint* dst)
{
	uint i = get_global_id(0);
	if (i < size)
		dst[i] = val;
}
)KERNELSRC";


int main(int argc, char** argv)
{
	try
	{
		auto rte = OCL::create_i915_rte("/dev/dri/card0");

		auto kernel = rte->compile_kernel(kernel_src, "cl_memset", "-cl-std=CL1.2");

		auto build_log = kernel->get_build_log();
		if (build_log.size() > 0)
			printf("Build log:\n%s\n", build_log.c_str());


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
