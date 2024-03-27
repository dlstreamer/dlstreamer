/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/dictionary.h"
#include "dlstreamer/element.h"
#include <gst/base/gstbasetransform.h>
#include <mutex>

namespace dlstreamer {

namespace param {
static constexpr auto shared_instance_id = "shared-instance-id";
}

class SharedInstance {
  public:
    struct InstanceId {
        std::string name;
        std::string shared_instance_id;
        BaseDictionary params;
        FrameInfo input_info;
        FrameInfo output_info;

        inline bool operator<(const InstanceId &r) const {
            const InstanceId &l = *this;
            return std::tie(l.name, l.shared_instance_id, l.params, l.input_info, l.output_info) <
                   std::tie(r.name, r.shared_instance_id, r.params, r.input_info, r.output_info);
        }
    };

    ElementPtr init_or_reuse(const InstanceId &id, ElementPtr element, std::function<void()> init) {
        // This code path is work in progress and not active in DLStreamer architecture 1.0
        assert(false);

        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _shared_elements.find(id);
        if (it != _shared_elements.end()) {
            return it->second;
        } else {
            if (init)
                init();
            _shared_elements.insert({id, element});
            return element;
        }
    }

    void clean_up() {
        std::lock_guard<std::mutex> guard(_mutex);
        for (auto it = _shared_elements.cbegin(); it != _shared_elements.cend();) {
            if (it->second.use_count() == 1) { // no other references
                it = _shared_elements.erase(it);
            } else {
                ++it;
            }
        }
    }
    static SharedInstance *global() {
        static SharedInstance g_shared_instance;
        return &g_shared_instance;
    }

  private:
    std::map<InstanceId, ElementPtr> _shared_elements;
    std::mutex _mutex;
};

} // namespace dlstreamer
