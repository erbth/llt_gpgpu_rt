/** Standalone IGC invoker inspired by IGC usage in intel-compute-runtime*
 * (mainly by its offline compiler, i.e. ocloc)
 *
 * References:
 *   * intel-compute-runtime: https://github.com/intel/compute-runtime
 */
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "igc_interface.h"

using namespace std;


void main_exec_save()
{
	IGCInterface igc;
}

int main(int argc, char** argv)
{
	try
	{
		main_exec_save();
	}
	catch (const exception& e)
	{
		fprintf(stderr, "Error: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
