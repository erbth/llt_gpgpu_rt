/* vim: set ft=c: */
void __kernel test_pattern(uint width, uint height, uint pitch, uint pos,
		__global short* colormap, __global uint* dst)
{
	uint I = get_global_id(0);
	uint J = get_global_id(1);

	uint i = get_group_id(0);
	uint ii = get_local_id(0);

	if (I < width && J < height)
	{
		/* Determine color */
		uint stripe_height = height / 7;
		uint colormap_idx = J / stripe_height;

		uint r = colormap[colormap_idx * 4 + 0];
		uint g = colormap[colormap_idx * 4 + 1];
		uint b = colormap[colormap_idx * 4 + 2];

		/* Determine alpha */
		uint alpha = (I << 16) / width + pos;
		if (alpha >= 65536)
			alpha -= 65536;

		/* Adjust color according to alpha value */
		r = (r * alpha) & 0xff0000;
		g = ((g * alpha) >> 8) & 0xff00;
		b = ((b * alpha) >> 16) & 0xff;

		uint color = r | g | b;

		/* Fill pixel */
		uint y_offset = (J / 8) * pitch * 8 + (J % 8) * 128;
		dst[ii + i*1024 + y_offset] = color;
	}
}
