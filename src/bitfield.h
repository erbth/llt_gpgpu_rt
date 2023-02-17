#ifndef __BITFIELD_H
#define __BITFIELD_H

#include <type_traits>

/** An approach to defined-behavior bit-fields */
template<typename T, int I, int M, int N>
struct BitField
{
	static_assert(N >= M, "N must be >= M");

	T _base;
	static T constexpr MASK = ((1ULL << (N-M+1)) - 1ULL);

	inline T get()
	{
		auto v = reinterpret_cast<T*>(this)[I];
		return (v >> M) & MASK;
	}

	inline T set(T x)
	{
		auto& v = reinterpret_cast<T*>(this)[I];
		v = (v & ~MASK) | (x << M);
		return x;
	}
};


template<typename T, int I, int M, int N>
struct BitAccess
{
	static_assert(N >= M, "N must be >= M");

	static T constexpr MASK = ((1ULL << (N-M+1)) - 1ULL);

	static inline T get(const T* arr)
	{
		return (arr[I] >> M) & MASK;
	}

	static inline T set(T* arr, T x)
	{
		arr[I] = (arr[I] & ~MASK) | (x << M);
		return x;
	}
};

#endif /* __BITFIELD_H */
