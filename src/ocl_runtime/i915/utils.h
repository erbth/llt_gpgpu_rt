#ifndef __UTILITY_H
#define __UTILITY_H

#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <string>
#include <limits>

std::string to_hex_string(unsigned int u);

template<typename T, typename A>
inline T align_value(T val, A alignment)
{
	return ((val + alignment - 1) / alignment) * alignment;
}

template<typename T>
class DynamicBuffer final
{
protected:
	T* _ptr;
	size_t _size;

	inline static size_t align_size(size_t req)
	{
		size_t s = 1;
		while (s < req)
		{
			if (s > std::numeric_limits<size_t>::max() / 2)
				throw std::invalid_argument("size too big");

			s *= 2;
		}

		return s;
	}

public:
	DynamicBuffer(size_t initial_size)
	{
		_size = std::max(align_size(initial_size), (size_t) 64);

		_ptr = (T*) malloc(sizeof(T) * _size);
		if (!_ptr)
			throw std::system_error(errno, std::generic_category());

		memset(_ptr, 0, _size);
	}

	DynamicBuffer(const DynamicBuffer&) = delete;
	DynamicBuffer& operator=(const DynamicBuffer&) = delete;

	~DynamicBuffer()
	{
		free(_ptr);
	}

	size_t size() const
	{
		return _size;
	}

	T* ptr()
	{
		return _ptr;
	}

	void ensure_size(size_t new_size)
	{
		new_size = align_size(new_size);
		if (new_size <= _size)
			return;

		auto new_ptr = (T*) realloc(_ptr, new_size);
		if (!new_ptr)
			throw std::system_error(errno, std::generic_category());

		_ptr = new_ptr;

		memset(_ptr + _size, 0, new_size - _size);

		_size = new_size;
	}
};

#endif /* __UTILITY_H */
