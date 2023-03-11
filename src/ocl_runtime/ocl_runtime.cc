#include "ocl_runtime.h"

using namespace std;

namespace OCL
{

NDRange::NDRange(uint32_t x, uint32_t y, uint32_t z)
	: x(x), y(y), z(z)
{
}

Kernel::~Kernel()
{
}

PreparedKernel::~PreparedKernel()
{
}

RTE::~RTE()
{
}

}
