#ifndef __HW_INFO_H
#define __HW_INFO_H


/* List of supported device families
 * NOTE: When changing these (especially when adding older families), check that
 * the rest of the codebase supports the new families. */
#define SUPPORT_GEN9
#define SUPPORT_CFL
#define SUPPORT_GLK

/* HW info from intel-compute-runtime ("NEO") */
#include "third_party/hw_info/gen9/hw_info.h"


#endif /* __HW_INFO_H */
