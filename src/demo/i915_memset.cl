/* vim: set ft=c: */

void __kernel cl_memset(uint size, uint val, __global uint* dst)
{
	uint i = get_global_id(0);
	if (i < size)
		dst[i] = val;
}
