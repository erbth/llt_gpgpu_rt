#ifndef __HASH_H
#define __HASH_H

#include <cstdlib>
#include <cstdint>

/* @param size must be a multiple of 4 */
uint64_t gen_hash(const char* data, size_t size);

#endif /* __HASH_H */
