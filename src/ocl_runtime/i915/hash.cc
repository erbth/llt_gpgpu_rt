#include <stdexcept>
#include "hash.h"

using namespace std;

/* Copied/adapted from intel-graphics-compiler, IGC/AdaptorOCL/OCL/sp/sp_g8.cpp
 * */
#define HASH_JENKINS_MIX(a,b,c)    \
{                                  \
a -= b; a -= c; a ^= (c>>13);  \
b -= c; b -= a; b ^= (a<<8);   \
c -= a; c -= b; c ^= (b>>13);  \
a -= b; a -= c; a ^= (c>>12);  \
b -= c; b -= a; b ^= (a<<16);  \
c -= a; c -= b; c ^= (b>>5);   \
a -= b; a -= c; a ^= (c>>3);   \
b -= c; b -= a; b ^= (a<<10);  \
c -= a; c -= b; c ^= (b>>15);  \
}

uint64_t gen_hash(const char* data, size_t size)
{
	if (size % 4 != 0)
		throw invalid_argument("Size must be a multiple of 4");

	uint32_t a = 0x428a2f98;
	uint32_t hi = 0x71374491;
	uint32_t lo = 0xb5c0fbcf;

	auto ptr = (const uint32_t*) data;

	for (size /= 4; size > 0; --size)
	{
		a ^= *ptr++;
		HASH_JENKINS_MIX(a, hi, lo);
	}

	return ((uint64_t) hi) << 32 | lo;
}
