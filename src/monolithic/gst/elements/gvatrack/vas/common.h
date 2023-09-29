/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __VAS_COMMON_H__
#define __VAS_COMMON_H__

#include <cstdint>

#define OT_VERSION_MAJOR 1
#define OT_VERSION_MINOR 0
#define OT_VERSION_PATCH 0

#define VAS_EXPORT //__attribute__((visibility("default")))

namespace vas {

/**
 * @class Version
 *
 * Contains version information.
 */
class Version {
  public:
    /**
     * Constructor.
     *
     * @param[in] major Major version.
     * @param[in] minor Minor version.
     * @param[in] patch Patch version.
     */
    explicit Version(uint32_t major, uint32_t minor, uint32_t patch) : major_(major), minor_(minor), patch_(patch) {
    }

    /**
     * Returns major version.
     */
    uint32_t GetMajor() const noexcept {
        return major_;
    }

    /**
     * Returns minor version.
     */
    uint32_t GetMinor() const noexcept {
        return minor_;
    }

    /**
     * Returns patch version.
     */
    uint32_t GetPatch() const noexcept {
        return patch_;
    }

  private:
    uint32_t major_;
    uint32_t minor_;
    uint32_t patch_;
};

/**
 * @enum BackendType
 *
 * Represents HW backend types.
 */
enum class BackendType {
    CPU,  /**< CPU */
    GPU,  /**< GPU */
    VPU,  /**< VPU */
    FPGA, /**< FPGA */
    HDDL  /**< HDDL */
};

/**
 * @enum ColorFormat
 *
 * Represents Color formats.
 */
enum class ColorFormat { BGR, NV12, BGRX, GRAY, I420 };

}; // namespace vas

#endif // __VAS_COMMON_H__
