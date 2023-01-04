/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "third_party/hw_info/gen9/hw_info_gen9.h"

namespace NEO {

struct GLK : public SKLFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const uint64_t defaultHardwareInfoConfig;
    static const uint32_t threadsPerEu = 6;
    static const uint32_t maxEuPerSubslice = 6;
    static const uint32_t maxSlicesSupported = 1;
    static const uint32_t maxSubslicesSupported = 3;

    static const RuntimeCapabilityTable capabilityTable;

    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
};

class GLK_1x3x6 : public GLK {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};

class GLK_1x2x6 : public GLK {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};
} // namespace NEO
