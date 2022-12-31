/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "third_party/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_GEMINILAKE>::abbreviation = "glk";

bool isSimulationGLK(unsigned short deviceId) {
    return false;
};

const PLATFORM GLK::platform = {
    IGFX_GEMINILAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_MOBILE, // default init
    0,               // usDeviceID
    0,               // usRevId. 0 sets the stepping to A0
    0,               // usDeviceID_PCH
    0,               // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable GLK::capabilityTable{
    52.083,                                        // defaultProfilingTimerResolution
    "lp",                                          // platformType
    "",                                            // deviceName
    30,                                            // clVersionSupport
    64,                                            // slmSize
};

WorkaroundTable GLK::workaroundTable = {};
FeatureTable GLK::featureTable = {};

void GLK::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->flags.ftr3dMidBatchPreempt = true;
    featureTable->flags.ftr3dObjectLevelPreempt = true;
    featureTable->flags.ftrPerCtxtPreemptionGranularityControl = true;
    featureTable->flags.ftrLCIA = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;
    featureTable->flags.ftrEnableGuC = true;
    featureTable->flags.ftrTileMappedResource = true;
    featureTable->flags.ftrULT = true;
    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;
    featureTable->flags.ftrTileY = true;

    workaroundTable->flags.waLLCCachingUnsupported = true;
    workaroundTable->flags.waMsaa8xTileYDepthPitchAlignment = true;
    workaroundTable->flags.waFbcLinearSurfaceStride = true;
    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->flags.waSendMIFLUSHBeforeVFE = true;
    workaroundTable->flags.waForcePcBbFullCfgRestore = true;
    workaroundTable->flags.waReportPerfCountUseGlobalContextID = true;
    workaroundTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

const HardwareInfo GLK_1x3x6::hwInfo = {
    &GLK::platform,
    &GLK::featureTable,
    &GLK::workaroundTable,
    &GLK_1x3x6::gtSystemInfo,
    GLK::capabilityTable,
};

GT_SYSTEM_INFO GLK_1x3x6::gtSystemInfo = {0};
void GLK_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * GLK::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = GLK::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = GLK::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = GLK::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo GLK_1x2x6::hwInfo = {
    &GLK::platform,
    &GLK::featureTable,
    &GLK::workaroundTable,
    &GLK_1x2x6::gtSystemInfo,
    GLK::capabilityTable,
};
GT_SYSTEM_INFO GLK_1x2x6::gtSystemInfo = {0};
void GLK_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * GLK::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = GLK::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = GLK::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = GLK::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo GLK::hwInfo = GLK_1x3x6::hwInfo;
const uint64_t GLK::defaultHardwareInfoConfig = 0x100030006;

void setupGLKHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100020006) {
        GLK_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030006) {
        GLK_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        GLK_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*GLK::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupGLKHardwareInfoImpl;
} // namespace NEO
