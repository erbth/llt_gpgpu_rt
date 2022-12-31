/*
 * Parts collected from gen9/hw_info_gen9.h and gen9/hw_cmds_base.h
 *
 * hw_info_gen9.h:
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * hw_cmds_base.h:
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "third_party/helpers/hw_info.h"

#include <cstddef>
#include <igfxfmid.h>

namespace NEO {

struct SKLFamily;

template <>
struct GfxFamilyMapper<IGFX_GEN9_CORE> {
    typedef SKLFamily GfxFamily;
    static const char *name;
};

struct GEN9 {
    static constexpr bool supportsSampler = true;
    static constexpr bool isUsingGenericMediaStateClear = true;
    static constexpr bool isUsingMiMemFence = false;

    struct DataPortBindlessSurfaceExtendedMessageDescriptor {
        union {
            struct {
                uint32_t bindlessSurfaceOffset : 20;
                uint32_t reserved : 1;
                uint32_t executionUnitExtendedMessageDescriptorDefinition : 11;
            };
            uint32_t packed;
        };

        DataPortBindlessSurfaceExtendedMessageDescriptor() {
            packed = 0;
        }

        void setBindlessSurfaceOffset(uint32_t offsetInBindlessSurfaceHeapInBytes) {
            bindlessSurfaceOffset = offsetInBindlessSurfaceHeapInBytes >> 6;
        }

        uint32_t getBindlessSurfaceOffsetToPatch() {
            return bindlessSurfaceOffset << 12;
        }
    };

    static_assert(sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor) == sizeof(DataPortBindlessSurfaceExtendedMessageDescriptor::packed), "");
};

struct SKLFamily : public GEN9 {
    using GfxFamily = SKLFamily;
};

} // namespace NEO
