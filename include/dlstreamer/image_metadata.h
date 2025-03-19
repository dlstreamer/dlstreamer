/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/dictionary.h"
#include "dlstreamer/image_info.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

class TimestampMetadata {
  public:
    static constexpr auto name = "timestamp";
    struct key {
        static constexpr auto pts = "pts";           // (intptr_t) nano-seconds
        static constexpr auto dts = "dts";           // (intptr_t) nano-seconds
        static constexpr auto duration = "duration"; // (intptr_t) nano-seconds
    };
};

class InferenceResultMetadata : public DictionaryProxy {
  public:
    static constexpr auto name = "tensor";
    struct key {
        static constexpr auto model_name = "model_name"; // std::string
        static constexpr auto layer_name = "layer_name"; // std::string
        static constexpr auto format = "format";         // std::string
        // tensor representation
        static constexpr auto data_buffer = "data_buffer"; // array: {void*, size_t}
        static constexpr auto dims = "dims";               // std::vector<size_t>
        static constexpr auto precision = "precision";     // GVAPrecision
    };
    using DictionaryProxy::DictionaryProxy;

    TensorPtr tensor() const {
        auto data_buffer = try_get_array(key::data_buffer);
        if (!data_buffer.first)
            throw std::runtime_error("Error getting data_buffer");
        return std::make_shared<TensorImpl>(const_cast<void *>(data_buffer.first), info());
    }
    inline std::string model_name() const {
        return _dict->get<std::string>(key::model_name);
    }
    inline std::string layer_name() const {
        return _dict->get<std::string>(key::layer_name);
    }
    inline std::string format() const {
        return _dict->get<std::string>(key::format, "");
    }

    void set_model_name(const std::string &model_name) {
        set(key::model_name, model_name);
    }
    void set_layer_name(const std::string &layer_name) {
        set(key::layer_name, layer_name);
    }

    void init_tensor_data(const Tensor &tensor, const std::string &layer_name = "", const std::string &format = "") {
        set_info(tensor.info());
        set_array(key::data_buffer, tensor.data(), tensor.info().nbytes());
        if (!layer_name.empty())
            set_layer_name(layer_name);
        if (!format.empty())
            set(key::format, format);
    }

  private:
    typedef enum {
        GVA_PRECISION_FP32 = 10,
        GVA_PRECISION_U8 = 40,
        GVA_PRECISION_I32 = 70,
        GVA_PRECISION_I64 = 72,
        GVA_PRECISION_UNSPECIFIED = 255,
    } GVAPrecision;
    typedef enum {
        GVA_LAYOUT_ANY = 0,
        GVA_LAYOUT_NCHW = 1,
        GVA_LAYOUT_NHWC = 2,
    } GVALayout;
    static int dtype_to_gva(DataType dtype) {
        switch (dtype) {
        case DataType::UInt8:
            return GVA_PRECISION_U8;
        case DataType::Float32:
            return GVA_PRECISION_FP32;
        case DataType::Int32:
            return GVA_PRECISION_I32;
        case DataType::Int64:
            return GVA_PRECISION_I64;
        }
        return GVA_PRECISION_UNSPECIFIED;
    }
    static DataType dtype_from_gva(int precision) {
        switch (precision) {
        case GVA_PRECISION_U8:
            return DataType::UInt8;
        case GVA_PRECISION_FP32:
            return DataType::Float32;
        case GVA_PRECISION_I32:
            return DataType::Int32;
        case GVA_PRECISION_I64:
            return DataType::Int64;
        }
        throw std::runtime_error("Unknown GVAPrecision");
    }
    static int layout_to_gva(ImageLayout layout) {
        switch (layout) {
        case ImageLayout::NCHW:
            return GVA_LAYOUT_NCHW;
        case ImageLayout::NHWC:
            return GVA_LAYOUT_NHWC;
        default:
            return GVA_LAYOUT_ANY;
        }
    }
    TensorInfo info() const {
        auto dtype = dtype_from_gva(get<int>(key::precision));
        auto shape = get<std::vector<size_t>>(key::dims);
        return TensorInfo(shape, dtype);
    }
    void set_info(const TensorInfo &info) {
        auto shape = info.shape;
        auto stride = info.stride;
        // 3-dims to 4-dims
        if (shape.size() == 3) {
            shape.insert(shape.begin(), 1);
            stride.insert(stride.begin(), *stride.begin());
        }
        set(key::precision, dtype_to_gva(info.dtype));
        set(key::dims, shape);
        set("layout", layout_to_gva(ImageLayout(shape)));
    }

    class TensorImpl : public Tensor {
      public:
        TensorImpl(void *data, const TensorInfo &info) : _data(data), _info(info) {
        }
        const TensorInfo &info() const override {
            return _info;
        }
        MemoryType memory_type() const override {
            return MemoryType::CPU;
        }
        virtual ContextPtr context() const override {
            return nullptr;
        }
        void *data() const override {
            return _data;
        }
        handle_t handle(std::string_view /*key*/) const override {
            return 0;
        }
        handle_t handle(std::string_view /*key*/, handle_t /*default_value*/) const noexcept override {
            return 0;
        }
        TensorPtr parent() const override {
            return nullptr;
        }

      private:
        void *_data;
        const TensorInfo _info;
    };
};

class DetectionMetadata : public InferenceResultMetadata {
  public:
    static constexpr auto name = "detection";
    struct key {
        static constexpr auto x_min = "x_min";           // double
        static constexpr auto y_min = "y_min";           // double
        static constexpr auto x_max = "x_max";           // double
        static constexpr auto y_max = "y_max";           // double
        static constexpr auto confidence = "confidence"; // double
        static constexpr auto id = "id";                 // int
        static constexpr auto parent_id = "parent_id";   // int
        static constexpr auto label_id = "label_id";     // int
        static constexpr auto label = "label";           // std::string
    };
    using InferenceResultMetadata::InferenceResultMetadata;

    inline double x_min() const {
        return _dict->get<double>(key::x_min);
    }
    inline double y_min() const {
        return _dict->get<double>(key::y_min);
    }
    inline double x_max() const {
        return _dict->get<double>(key::x_max);
    }
    inline double y_max() const {
        return _dict->get<double>(key::y_max);
    }
    inline double confidence() const {
        return _dict->get<double>(key::confidence);
    }
    inline int id() const {
        return _dict->get<int>(key::id, -1);
    }
    inline int parent_id() const {
        return _dict->get<int>(key::parent_id, -1);
    }
    inline int label_id() const {
        return _dict->get<int>(key::label_id, -1);
    }
    inline std::string label() const {
        return _dict->get(key::label, std::string());
    }
    void init(double x_min, double y_min, double x_max, double y_max, double confidence = 0.0, int label_id = -1,
              std::string_view label = {}) {
        _dict->set(key::x_min, x_min);
        _dict->set(key::y_min, y_min);
        _dict->set(key::x_max, x_max);
        _dict->set(key::y_max, y_max);
        if (confidence)
            _dict->set(key::confidence, confidence);
        if (label_id >= 0)
            _dict->set(key::label_id, label_id);
        if (!label.empty())
            _dict->set(key::label, std::string(label));
    }
};

class ClassificationMetadata : public InferenceResultMetadata {
  public:
    static constexpr auto name = "classification";
    struct key {
        static constexpr auto label = "label";           // std::string
        static constexpr auto label_id = "label_id";     // int
        static constexpr auto confidence = "confidence"; // double
    };
    using InferenceResultMetadata::InferenceResultMetadata;

    inline std::string label() const {
        return _dict->get<std::string>(key::label);
    }
    inline int label_id() const {
        return _dict->get<int>(key::label_id);
    }
    inline double confidence() const {
        return _dict->get<double>(key::confidence);
    }
    void set_label(const std::string &label) {
        return _dict->set(key::label, label);
    }
    void set_label_id(int label_id) {
        _dict->set(key::label_id, label_id);
    }
    void set_confidence(double confidence) {
        _dict->set(key::confidence, confidence);
    }
};

class ObjectIdMetadata : public DictionaryProxy {
  public:
    static constexpr auto name = "object_id";
    struct key {
        static constexpr auto id = "id"; // int
    };
    using DictionaryProxy::DictionaryProxy;

    inline int id() const {
        return _dict->get<int>(key::id);
    }
    void set_id(int id) {
        _dict->set(key::id, id);
    }
};

class AffineTransformInfoMetadata : public DictionaryProxy {
  public:
    static constexpr auto name = "AffineTransformMetadata";
    struct key {
        static constexpr auto matrix = "matrix"; // std::vector<double>(6), affine transform matrix 2x3
    };
    using DictionaryProxy::DictionaryProxy;

    inline std::vector<double> matrix() const {
        return _dict->get<std::vector<double>>(key::matrix);
    }
    void set_matrix(const std::vector<double> &matrix) {
        _dict->set(key::matrix, matrix);
    }
    // From src/dst size and ROI, calculate matrix for converting full-frame normalized coordinates (dst to src)
    template <class RECT>
    void set_rect(double src_w, double src_h, double dst_w, double dst_h, const RECT &src_rect, const RECT &dst_rect) {
        std::vector<double> mat(6);
        // SRC_X = ((DST_X*dst_w - dst_rect.x)/dst_rect.width*src_rect.width + src_rect.x)/src_w
        assert(src_w * dst_rect.width != 0 && "Division by zero!");
        mat[0] = (dst_w * src_rect.width) / (src_w * dst_rect.width);
        mat[1] = 0.;
        mat[2] = src_rect.x / src_w - (dst_rect.x * src_rect.width) / (dst_rect.width * src_w);
        // SRC_Y = ((DST_Y*dst_h - dst_rect.y)/dst_rect.height*src_rect.height + src_rect.y)/src_h
        mat[3] = 0.;
        mat[4] = (dst_h * src_rect.height) / (src_h * dst_rect.height);
        mat[5] = src_rect.y / src_h - (dst_rect.y * src_rect.height) / (dst_rect.height * src_h);
        set_matrix(mat);
    }
};

class SourceIdentifierMetadata : public DictionaryProxy {
  public:
    static constexpr auto name = "SourceIdentifierMetadata";
    struct key {
        static constexpr auto batch_index = "batch_index"; // int
        static constexpr auto pts = "pts";                 // intptr_t (nanoseconds)
        static constexpr auto stream_id = "stream_id";     // intptr_t
        static constexpr auto roi_id = "roi_id";           // int
        static constexpr auto object_id = "object_id";     // int
    };
    using DictionaryProxy::DictionaryProxy;

    static std::shared_ptr<SourceIdentifierMetadata> try_cast(DictionaryPtr dict) {
        if (!dict || dict->name() != name)
            return nullptr;
        return std::make_shared<SourceIdentifierMetadata>(dict);
    }

    inline int batch_index() const {
        return _dict->get<int>(key::batch_index);
    }
    inline int64_t pts() const {
        return _dict->get<intptr_t>(key::pts);
    }
    inline intptr_t stream_id() const {
        return _dict->get<intptr_t>(key::stream_id);
    }
    inline int roi_id() const {
        return _dict->get<int>(key::roi_id, 0);
    }
    inline int object_id() const {
        return _dict->get<int>(key::object_id, 0);
    }

    void init(int batch_index, int64_t pts, intptr_t stream_id, int roi_id, int object_id = 0) {
        _dict->set(key::batch_index, batch_index);
        _dict->set(key::pts, static_cast<intptr_t>(pts));
        _dict->set(key::stream_id, stream_id);
        _dict->set(key::roi_id, roi_id);
        _dict->set(key::object_id, object_id);
    }
};

class ModelInfoMetadata : public DictionaryProxy {
  public:
    static constexpr auto name = "model_info";
    struct key {
        static constexpr auto model_name = "model_name"; // std::string
    };
    using DictionaryProxy::DictionaryProxy;

    inline std::string model_name() const {
        return _dict->get<std::string>(key::model_name);
    }
    FrameInfo input() {
        return get_info("input");
    };
    FrameInfo output() {
        return get_info("output");
    };
    std::vector<std::string> input_layers() {
        return layers("input");
    };
    std::vector<std::string> output_layers() {
        return layers("output");
    };

    void set_model_name(const std::string &model_name) {
        set(key::model_name, model_name);
    }
    void set_info(const std::string &info_name, const FrameInfo &info) {
        std::string datatype_str;
        std::string shapes_str;
        for (size_t i = 0; i < info.tensors.size(); i++) {
            if (i) {
                datatype_str += ",";
                shapes_str += ",";
            }
            datatype_str += datatype_to_string(info.tensors[i].dtype);
            shapes_str += shape_to_string(info.tensors[i].shape);
        }
        set(info_name + "_types", datatype_str);
        set(info_name + "_shapes", shapes_str);
    }
    FrameInfo get_info(const std::string info_name) {
        FrameInfo info;
        auto types_array = split_string(get<std::string>(info_name + "_types"), ',');
        auto shapes_array = split_string(get<std::string>(info_name + "_shapes"), ',');
        for (size_t i = 0;; i++) {
            if (i >= types_array.size() || i >= shapes_array.size())
                break;
            info.tensors.push_back({shape_from_string(shapes_array[i]), datatype_from_string(types_array[i])});
        }
        return info;
    }
    void set_layer_names(const std::string info_name, std::vector<std::string> layer_names) {
        set(info_name + "_names", join_strings(layer_names.cbegin(), layer_names.cend(), ','));
    }
    std::vector<std::string> layers(const std::string info_name) {
        return split_string(get<std::string>(info_name + "_names"), ',');
    }
};

} // namespace dlstreamer
