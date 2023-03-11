/** Heavily inspired by OpenCL */
#ifndef __OCL_RUNTIME_H
#define __OCL_RUNTIME_H

#include <cstdint>
#include <memory>
#include <string>

namespace OCL
{

struct NDRange final
{
public:
	const uint32_t x;
	const uint32_t y;
	const uint32_t z;

	NDRange(uint32_t x, uint32_t y = 1, uint32_t z = 1);
};

class Kernel
{
public:
	virtual ~Kernel() = 0;

	virtual std::string get_build_log() = 0;
};

class PreparedKernel
{
public:
	virtual ~PreparedKernel() = 0;

	virtual void add_argument(uint32_t) = 0;
	virtual void add_argument(int32_t) = 0;
	virtual void add_argument(uint64_t) = 0;
	virtual void add_argument(int64_t) = 0;

	/* @param size is in bytes */
	virtual void add_argument(void*, size_t) = 0;

	virtual void execute(NDRange global_size, NDRange local_size) = 0;
};

/* Runtime environment */
class RTE
{
public:
	virtual ~RTE() = 0;

	virtual std::shared_ptr<Kernel> compile_kernel(const char* src,
			const char* name, const char* options) = 0;

	virtual std::unique_ptr<PreparedKernel> prepare_kernel(std::shared_ptr<Kernel> kernel) = 0;
};

}

#endif /* __OCL_RUNTIME_H */
