/* vim: set ft=c: */

void __kernel cl_memset_slm(uint size, uint val, __global uint* dst)
{
	uint i = get_global_id(0);
	__local uint slm[256];

	if (i < size)
		slm[i] = val;

	barrier(CLK_LOCAL_MEM_FENCE);

	if (i < size)
		dst[i] = slm[i];

	if (i < size)
		dst[i] = val;
}
