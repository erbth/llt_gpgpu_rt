/** Translate between IGC and MESA device information */
#ifndef __TRANSLATE_INTERFACES_H
#define __TRANSLATE_INTERFACES_H

#include "../third_party/mesa/intel_device_info.h"
#include <igfxfmid.h>

namespace OCL
{

PRODUCT_FAMILY get_product_family(const struct intel_device_info& dev_info);
enum intel_platform get_intel_platform(PRODUCT_FAMILY family);
GFXCORE_FAMILY get_render_core_family(const struct intel_device_info& dev_info);

}

#endif /* __TRANSLATE_INTERFACES_H */
