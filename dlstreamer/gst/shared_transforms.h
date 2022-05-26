/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/transform.h"
#include <gst/base/gstbasetransform.h>
#include <mutex>

namespace dlstreamer {

class SharedTransforms {
  public:
    struct InstanceId {
        std::string name;
        std::string shared_instance_id;
        STDDictionary params;
        BufferInfo input_info;
        BufferInfo output_info;

        inline bool operator<(const InstanceId &r) const {
            const InstanceId &l = *this;
            return std::tie(l.name, l.shared_instance_id, l.params, l.input_info, l.output_info) <
                   std::tie(r.name, r.shared_instance_id, r.params, r.input_info, r.output_info);
        }
    };

    TransformBasePtr init_or_reuse(const InstanceId &id, TransformBasePtr transform) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto it = _shared_transforms.find(id);
        if (it != _shared_transforms.end()) {
            return it->second;
        } else {
            transform->set_info(id.input_info, id.output_info);
            _shared_transforms.insert({id, transform});
            return transform;
        }
    }

    void clean_up() {
        for (auto it = _shared_transforms.cbegin(); it != _shared_transforms.cend();) {
            if (it->second.use_count() == 1) { // no other references
                _shared_transforms.erase(it);
            }
            it++;
        }
    }
    static SharedTransforms *global() {
        static SharedTransforms g_shared_tranforms;
        return &g_shared_tranforms;
    }

  private:
    std::map<InstanceId, TransformBasePtr> _shared_transforms;
    std::mutex _mutex;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

template <class Key, class Value>
class MultiValueStorage {
  public:
    void add(Key key, Value value) {
        std::lock_guard<std::mutex> guard(_mutex);
        _values[key].push_back(value);
    }
    void remove(Key key, Value value) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto &vec = _values[key];
        auto it = std::find(vec.begin(), vec.end(), value);
        if (it != vec.end()) {
            vec.erase(it);
        }
    }
    Value get_first(Key key) {
        std::lock_guard<std::mutex> guard(_mutex);
        return *_values[key].begin();
    }

  private:
    std::map<Key, std::vector<Value>> _values;
    std::mutex _mutex;
};

extern MultiValueStorage<TransformBase *, GstBaseTransform *> g_gst_base_transform_storage;

} // namespace dlstreamer
