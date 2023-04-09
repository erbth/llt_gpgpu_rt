#include <llt_gpgpu_rt/i915_compiled_program.h>
#include "third_party/mesa/intel_device_info.h"

using namespace std;

namespace OCL
{

/* Ensure enum size */
static_assert(sizeof(enum intel_platform) == sizeof(int));

I915CompiledProgram::~I915CompiledProgram()
{
}

}
