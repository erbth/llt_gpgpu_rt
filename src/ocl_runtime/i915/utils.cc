#include <cstdio>
#include <utils.h>

using namespace std;

string to_hex_string(unsigned int u)
{
	char buf[16];

	snprintf(buf, 16, "0x%x", u);
	buf[15] = '\0';

	return string(buf);
}
