/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <gtsysinfo.h>
#include <igfxfmid.h>
#include "sku_info_base.h"

#include <cstddef>
#include <string>

namespace NEO {

struct RuntimeCapabilityTable {
    double defaultProfilingTimerResolution;
    const char *platformType;
    const char *deviceName;
    unsigned int clVersionSupport;
    uint32_t slmSize;
};

struct HardwareInfo {
    PLATFORM platform = {};
    FeatureTable featureTable = {};
    WorkaroundTable workaroundTable = {};
    alignas(4) GT_SYSTEM_INFO gtSystemInfo = {};

    alignas(8) RuntimeCapabilityTable capabilityTable = {};
};

template <PRODUCT_FAMILY product>
struct HwMapper {};

template <GFXCORE_FAMILY gfxFamily>
struct GfxFamilyMapper {};

void setHwInfoValuesFromConfig(const uint64_t hwInfoConfig, HardwareInfo &hwInfoIn);
} // namespace NEO
