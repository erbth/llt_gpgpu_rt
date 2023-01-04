/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/helpers/hw_info.h"

namespace NEO {
void setHwInfoValuesFromConfig(const uint64_t hwInfoConfig, HardwareInfo &hwInfoIn) {
    uint32_t sliceCount = static_cast<uint16_t>(hwInfoConfig >> 32);
    uint32_t subSlicePerSliceCount = static_cast<uint16_t>(hwInfoConfig >> 16);
    uint32_t euPerSubSliceCount = static_cast<uint16_t>(hwInfoConfig);

    hwInfoIn.gtSystemInfo.SliceCount = sliceCount;
    hwInfoIn.gtSystemInfo.SubSliceCount = subSlicePerSliceCount * sliceCount;
    hwInfoIn.gtSystemInfo.DualSubSliceCount = subSlicePerSliceCount * sliceCount;
    hwInfoIn.gtSystemInfo.EUCount = euPerSubSliceCount * subSlicePerSliceCount * sliceCount;
    for (uint32_t slice = 0; slice < hwInfoIn.gtSystemInfo.SliceCount; slice++) {
        hwInfoIn.gtSystemInfo.SliceInfo[slice].Enabled = true;
    }
}
} // namespace NEO
