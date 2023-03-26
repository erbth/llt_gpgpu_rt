#ifndef __I915_KERNEL_UTILS_H
#define __I915_KERNEL_UTILS_H

#include <stdexcept>
#include <memory>
#include <vector>
#include "../ocl_runtime.h"
#include "igc_progbin.h"

namespace OCL {

/* Prototoypes of mutually required header files */
class I915RTEImpl;

class KernelArg
{
public:
	virtual ~KernelArg() = 0;
};

template <typename T>
class KernelArgInt : public KernelArg
{
protected:
	T _value;

public:
	KernelArgInt(T value)
		: _value(value)
	{
	}

	~KernelArgInt()
	{
	}

	T value() const
	{
		return _value;
	}
};

class KernelArgPtr : public KernelArg
{
protected:
	void* _ptr;
	const size_t _size;

public:
	KernelArgPtr(size_t page_size, void* ptr, size_t size);
	~KernelArgPtr();

	void* ptr();
	size_t size() const;
};

class KernelArgGEMName : public KernelArg
{
protected:
	I915RTEImpl& rte;
	uint32_t _handle;
	size_t _size;

public:
	KernelArgGEMName(I915RTEImpl& rte, uint32_t name);
	~KernelArgGEMName();

	uint32_t handle() const;
	size_t size() const;
};

class I915RingCmd
{
public:
	virtual ~I915RingCmd() = 0;

	virtual size_t bin_size() const = 0;
	virtual size_t bin_write(char* dst) const = 0;
};

/* Return type is a signed integer s.t. right-shifts preserve the properties of
 * a canonical address */
inline int64_t canonical_address(uint64_t addr)
{
	return (addr << 16) >> 16;
}

inline int64_t canonical_address(void* addr)
{
	return canonical_address((uint64_t) (uintptr_t) addr);
}

inline uint32_t slm_size_from_idesc(uint32_t s)
{
	if (s == 0)
		return 0;

	if (s > 7)
		throw std::invalid_argument("idesc SLM size > 7");

	uint32_t v = 1024;
	while (--s > 0)
		v *= 2;

	return v;
}

inline uint32_t slm_size_to_idesc(uint32_t v)
{
	if (v == 0)
		return 0;

	if (v > 64 * 1024)
		throw std::invalid_argument("SLM size > 64kiB");

	uint32_t s = 1;
	uint32_t v_prime = 1024;

	while (v_prime < v)
	{
		v_prime *= 2;
		s += 1;
	}

	return s;
}

/* Invoke with @param dst = nullptr and @param capacity = 0 to determine the
 * required buffer size. */
size_t build_cross_thread_data(
		const KernelParameters& params,
		const NDRange& global_offset,
		const NDRange& local_size,
		const std::vector<std::unique_ptr<KernelArg>>& args,
		const char* surface_state_base, size_t surface_state_size,
		char* dst, size_t capacity);

}

#endif /* __I915_KERNEL_UTILS_H */
