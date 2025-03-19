/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __COMMON_EXCEPTION_H__
#define __COMMON_EXCEPTION_H__

#include "vas/common.h"
#include <exception>
#include <stdexcept>

#define ETHROW(condition, exception_class, message, ...)                                                               \
    {                                                                                                                  \
        if (!(condition)) {                                                                                            \
            throw std::exception_class(message);                                                                       \
        }                                                                                                              \
    }

#define TRACE(fmt, ...)

#endif // __COMMON_EXCEPTION_H__
