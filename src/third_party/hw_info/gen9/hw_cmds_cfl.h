/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "third_party/hw_info/gen9/hw_info_gen9.h"

namespace NEO {

struct CFL : public SKLFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const uint64_t defaultHardwareInfoConfig;
    static const uint32_t threadsPerEu = 7;
    static const uint32_t maxEuPerSubslice = 8;
    static const uint32_t maxSlicesSupported = 3;
    static const uint32_t maxSubslicesSupported = 9;

    static const RuntimeCapabilityTable capabilityTable;

    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
};

class CFL_1x2x6 : public CFL {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};

class CFL_1x3x6 : public CFL {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};

class CFL_1x3x8 : public CFL {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};

class CFL_2x3x8 : public CFL {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};

class CFL_3x3x8 : public CFL {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;
};
} // namespace NEO
