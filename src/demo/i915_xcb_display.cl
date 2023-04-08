/* vim: set ft=c: */
void __kernel fill_drawable(uint size, uint val, __global uint* dst)
{
	uint i = get_global_id(0);
	if (i < size)
		dst[i] = val;
}

void __kernel test_pattern(uint width, uint height, uint pitch, __global uint* vals, __global uint* dst)
{
	uint I = get_global_id(0);
	uint J = get_global_id(1);

	uint i = get_group_id(0);
	uint ii = get_local_id(0);

	if (I < width && J < height)
	{
		uint y_offset = (J / 8) * pitch * 8 + (J % 8) * 128;
		dst[ii + i*1024 + y_offset] = vals[(I * 6) / width];
	}
}

void __kernel display_irct(
	uint width, uint height, uint dst_pitch, uint src_pitch,
	__global short* src_y, __global short* src_cb, __global short* src_cr,
	__global uint* dst)
{
	uint I = get_global_id(0);
	uint J = get_global_id(1);

	uint i = get_group_id(0);
	uint ii = get_local_id(0);

	if (I < width && J < height)
	{
		int src_offset = J*src_pitch + I;
		int y = src_y[src_offset];
		int cb = src_cb[src_offset];
		int cr = src_cr[src_offset];

		int g = y - ((cb + cr) >> 2);
		int b = g + cb;
		int r = g + cr;

		r <<= 16;
		g <<= 8;

		uint y_offset = (J / 8) * dst_pitch * 8 + (J % 8) * 128;
		dst[ii + i*1024 + y_offset] = r | g | b;
	}
}
