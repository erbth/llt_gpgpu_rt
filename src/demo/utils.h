#ifndef __DEMO_UTILS_H
#define __DEMO_UTILS_H

#include <cerrno>
#include <system_error>

#define DIV_ROUND_UP(X, Y) (((X) + (Y) - 1) / (Y))

class AlignedBuffer
{
protected:
	char* _ptr;
	size_t _size;

public:
	AlignedBuffer(size_t alignment, size_t size_arg)
		: _size(((size_arg + alignment - 1) / alignment) * alignment)
	{
		_ptr = (char*) aligned_alloc(alignment, _size);
		if (!_ptr)
			throw std::system_error(errno, std::generic_category(), "Failed to allocate aligned memory");
	}

	AlignedBuffer(const AlignedBuffer&) = delete;
	AlignedBuffer& operator=(const AlignedBuffer&) = delete;

	~AlignedBuffer()
	{
		free(_ptr);
	}

	char* ptr()
	{
		return _ptr;
	}

	size_t size()
	{
		return _size;
	}
};

#endif /* __DEMO_UTILS_H */
