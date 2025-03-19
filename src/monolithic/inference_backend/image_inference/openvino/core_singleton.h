/*******************************************************************************
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

class IeCoreSingleton {
  private:
    IeCoreSingleton(){};
    IeCoreSingleton(const IeCoreSingleton &other) = delete;
    IeCoreSingleton &operator=(const IeCoreSingleton &other) = delete;
};
