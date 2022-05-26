/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/transform.h"
#include "dlstreamer/buffer_mappers/gst_to_cpu.h"
#include "dlstreamer/buffer_mappers/gst_to_opencl.h"
#include "dlstreamer/buffer_mappers/gst_to_vaapi.h"
#include "dlstreamer/gst/transform.h"
#include "dlstreamer/metadata.h"
#include "dlstreamer/utils.h"

#include "allocator.h"
#include "opencl_context.h"
#include "pool.h"
#include "shared_transforms.h"
#include "source_id.h"
#include "vaapi_context.h"

//#include <inference_backend/logger.h>
#ifndef ITT_TASK
#define ITT_TASK(NAME)
#endif

#define BUFFER_POOL_SIZE_DEFAULT 16

namespace dlstreamer {

template <auto>
struct Callback;

template <class T, class Ret, class... Args, Ret (T::*mem_fn)(Args...)>
struct Callback<mem_fn> {
    static Ret fn(GObject *obj, Args... args) {
        return (T::unpack(obj)->*mem_fn)(std::forward<Args>(args)...);
    }

    static Ret fn(GstBaseTransform *obj, Args... args) {
        return (T::unpack(obj)->*mem_fn)(std::forward<Args>(args)...);
    }
};

static ContextPtr query_context(GstPad *pad, std::string_view name) {
    if (name == VAAPIContext::context_name)
        return std::make_shared<GSTVAAPIContext>(pad);
    if (name == OpenCLContext::context_name)
        return std::make_shared<GSTOpenCLContext>(pad);
    return nullptr;
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...)->overloaded<Ts...>;

struct GstDlsTransformClass {
    GstBaseTransformClass base_class;
    TransformDesc *desc;
    gint private_offset;

    static void init(gpointer g_class, gpointer class_data);
};

class GstDlsTransform : public ITransformController {
  public:
    struct GstPodData {
        GstBaseTransform base;
        GstDlsTransform *instance;
    };

    static GstDlsTransform *unpack(GstBaseTransform *base) {
        assert(reinterpret_cast<GstPodData *>(base)->instance);
        return reinterpret_cast<GstPodData *>(base)->instance;
    }

    static GstDlsTransform *unpack(GObject *object) {
        return unpack(GST_BASE_TRANSFORM(object));
    }

    static void instance_init(GTypeInstance *instance, gpointer g_class) {
        auto *self = reinterpret_cast<GstPodData *>(instance);

        gpointer instance_memory = g_type_instance_get_private(instance, G_TYPE_FROM_INSTANCE(instance));
        self->instance = new (instance_memory) GstDlsTransform(&self->base, g_class);
    }

    static void instance_finalize(GObject *object) {
        auto self = reinterpret_cast<GstPodData *>(object);
        assert(self->instance);

        auto parent_class = self->instance->_parent_class;
        // Manually invoke object destruction since it was created via placement-new.
        self->instance->~GstDlsTransform();
        self->instance = nullptr;

        G_OBJECT_CLASS(parent_class)->finalize(object);
    }

    GstDlsTransform(GstBaseTransform *base, gpointer g_class)
        : _base(base), _class_data(static_cast<GstDlsTransformClass *>(g_class)) {
        _parent_class = GST_BASE_TRANSFORM_CLASS(g_type_class_peek_parent(_class_data));

        if (_class_data->desc->params) {
            for (auto const &param : *_class_data->desc->params) {
                _params->set(param.name, param.default_value);
            }
        }
    }

    ~GstDlsTransform() {
        SharedTransforms::global()->clean_up();
        g_gst_base_transform_storage.remove(_transform_base.get(), _base);
    }

    void get_property(guint property_id, GValue *value, GParamSpec *pspec);
    void set_property(guint property_id, const GValue *value, GParamSpec *pspec);
    gboolean query(GstPadDirection direction, GstQuery *query);

    gboolean start() {
        GST_DEBUG_OBJECT(_base, "start");
        try {
            _transform_base = _class_data->desc->create(*this, _params);
            if (!_transform_base) {
                GST_ELEMENT_ERROR(_base, LIBRARY, INIT, ("Invalid create function"),
                                  ("The create function returned null"));
            }
        } catch (std::exception &e) {
            GST_ELEMENT_ERROR(_base, LIBRARY, INIT, ("Couldn't create instance"),
                              ("The create function threw an exception: %s", e.what()));
        }

        if (!_transform_base)
            return false;

        _transform = dynamic_cast<Transform *>(_transform_base.get());
        _transform_with_alloc = dynamic_cast<TransformWithAlloc *>(_transform_base.get());
        _transform_inplace = dynamic_cast<TransformInplace *>(_transform_base.get());

        assert(_transform_inplace || _transform);
        gst_base_transform_set_in_place(_base, _transform_inplace ? true : false);
        return true;
    }

    gboolean set_caps(GstCaps *incaps, GstCaps *outcaps);
    GstCaps *transform_caps(GstPadDirection direction, GstCaps *caps, GstCaps *filter);
    GstFlowReturn generate_output(GstBuffer **outbuf);
    GstFlowReturn transform(GstBuffer *inbuf, GstBuffer *outbuf);
    GstFlowReturn transform_ip(GstBuffer *buf);

    //
    // ITransformController overrides
    //

    ContextPtr get_context(std::string_view name) override {
        ContextPtr context;
        for (auto &&pad : {_base->sinkpad, _base->srcpad}) {
            try {
                context = query_context(pad, name);
            } catch (...) {
            }
            if (context)
                break;
        }

        return context;
    }

    BufferMapperPtr create_input_mapper(BufferType buffer_type, ContextPtr context) override {
        // FIXME: test if buffer_type is equal to memory type of buffer on sink pad?
        switch (buffer_type) {
        case BufferType::CPU:
            return std::make_shared<BufferMapperGSTToCPU>();
        case BufferType::OPENCL_BUFFER:
            return std::make_shared<BufferMapperGSTToOpenCL>(context);
        case BufferType::VAAPI_SURFACE:
            return std::make_shared<BufferMapperGSTToVAAPI>(context);
        default:
            throw std::runtime_error(std::string("Unsupported buffer type to map from GST buffer type: ")
                                         .append(buffer_type_to_string(buffer_type)));
        }
    }

  private:
    void ensure_transform_ready() {
        if (_transform_ready)
            return;

        if (!_shared_instance_id.empty()) {
            auto params = dynamic_cast<STDDictionary *>(_params.get());
            if (!params)
                throw std::runtime_error("Properties shared-instance-id and params-structure can't be set together");
            SharedTransforms::InstanceId id = {std::string(_class_data->desc->name), _shared_instance_id, *params,
                                               _input_info, _output_info};
            _transform_base = SharedTransforms::global()->init_or_reuse(id, _transform_base);
            _transform = dynamic_cast<Transform *>(_transform_base.get());
            _transform_with_alloc = dynamic_cast<TransformWithAlloc *>(_transform_base.get());
            _transform_inplace = dynamic_cast<TransformInplace *>(_transform_base.get());
        } else {
            _transform_base->set_info(_input_info, _output_info);
        }

        if (_class_data->desc->flags & TRANSFORM_FLAG_MULTISTREAM_MUXER) {
            g_gst_base_transform_storage.add(_transform_base.get(), _base);
            try {
                GstContext *stream_id_ctx = gst_query_context(_base->srcpad, GSTStreamIdContext::context_name);
                const GstStructure *structure = gst_context_get_structure(stream_id_ctx);
                gst_structure_get(structure, GSTStreamIdContext::field_name, G_TYPE_POINTER, &_stream_id, NULL);
                gst_context_unref(stream_id_ctx);
            } catch (...) {
            }
        }

        _transform_ready = true;
    }

  private:
    GstBaseTransform *_base;
    GstDlsTransformClass *_class_data;
    GstBaseTransformClass *_parent_class;

    TransformBasePtr _transform_base;
    Transform *_transform = nullptr;
    TransformWithAlloc *_transform_with_alloc = nullptr;
    TransformInplace *_transform_inplace = nullptr;

    DictionaryPtr _params = std::make_shared<STDDictionary>();

    std::string _shared_instance_id;
    intptr_t _stream_id = 0;

    std::unique_ptr<Pool<dlstreamer::BufferPtr>> _pool;
    int _buffer_pool_size = BUFFER_POOL_SIZE_DEFAULT;
    BufferMapperPtr _output_mapper;
    BufferInfo _input_info;
    BufferInfo _output_info;

    bool _transform_ready = false;
};

void GstDlsTransform::get_property(guint property_id, GValue *value, GParamSpec *pspec) {
    const ParamDescVector &params_desc = _class_data->desc->params ? *_class_data->desc->params : ParamDescVector{};

    if (property_id == 0 || property_id > params_desc.size()) {
        // Test for additional properties related added by GstDlsTransform implementation
        if (std::string_view(pspec->name) == std::string_view(param::shared_instance_id)) {
            g_value_set_string(value, _shared_instance_id.c_str());
            return;
        }
        if (std::string_view(pspec->name) == std::string_view(param::buffer_pool_size)) {
            g_value_set_int(value, _buffer_pool_size);
            return;
        }
        if (std::string_view(pspec->name) == std::string_view(param::params_structure)) {
            const char *name = _params->name().empty() ? "params" : _params->name().data();
            GstStructure *structure = gst_structure_new_empty(name);
            GSTDictionary dict(structure);
            for (auto &&k : _params->keys()) {
                dict.set(k, *_params->try_get(k));
            }
            g_value_set_pointer(value, structure);
            return;
        }

        G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, property_id, pspec);
        return;
    }

    const ParamDesc &prmd = params_desc.at(property_id - 1);

    auto opt_val = _params->try_get(prmd.name);
    if (!opt_val)
        opt_val = prmd.default_value;

    std::visit(overloaded{[&](int param_val) { g_value_set_int(value, param_val); },
                          [&](double param_val) { g_value_set_double(value, param_val); },
                          [&](bool param_val) { g_value_set_boolean(value, param_val); },
                          [&](const std::string &param_val) { g_value_set_string(value, param_val.c_str()); },
                          [&](auto) { GST_ERROR_OBJECT(_base, "Unsupported value type of property"); }},
               *opt_val);
}

void GstDlsTransform::set_property(guint property_id, const GValue *value, GParamSpec *pspec) {
    const ParamDescVector &params_desc = _class_data->desc->params ? *_class_data->desc->params : ParamDescVector{};

    if (property_id == 0 || property_id > params_desc.size()) {
        // Test for additional properties related added by GstDlsTransform implementation
        if (std::string_view(pspec->name) == std::string_view(param::shared_instance_id)) {
            _shared_instance_id = g_value_get_string(value);
            return;
        }
        if (std::string_view(pspec->name) == std::string_view(param::buffer_pool_size)) {
            _buffer_pool_size = g_value_get_int(value);
            return;
        }
        if (std::string_view(pspec->name) == std::string_view(param::params_structure)) {
            GstStructure *structure = static_cast<GstStructure *>(g_value_get_pointer(value));
            _params = std::make_shared<GSTDictionary>(structure);
            return;
        }

        G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, property_id, pspec);
        return;
    }

    try {
        const ParamDesc &prmd = params_desc.at(property_id - 1);

        std::visit(
            overloaded{[&](int) { _params->set(prmd.name, g_value_get_int(value)); },
                       [&](double) { _params->set(prmd.name, g_value_get_double(value)); },
                       [&](bool) { _params->set(prmd.name, g_value_get_boolean(value) ? true : false); },
                       [&](const std::string &) { _params->set(prmd.name, std::string(g_value_get_string(value))); },
                       [&](auto) { GST_ERROR_OBJECT(_base, "Unsupported value type of property"); }},
            prmd.default_value);
    } catch (...) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, property_id, pspec);
    }
}

gboolean GstDlsTransform::query(GstPadDirection direction, GstQuery *query) {
    GST_DEBUG_OBJECT(_base, "query");

    if (GST_QUERY_TYPE(query) == GST_QUERY_CONTEXT) {
        const gchar *context_type;
        gst_query_parse_context_type(query, &context_type);

        ContextPtr ctx = _transform_base ? _transform_base->get_context(context_type) : ContextPtr();
        if (ctx) {
            GstContext *gst_ctx, *gst_ctx_old;
            GstStructure *s;

            gst_query_parse_context(query, &gst_ctx_old);

            if (gst_ctx_old)
                gst_ctx = gst_context_copy(gst_ctx_old);
            else
                gst_ctx = gst_context_new(context_type, FALSE);

            s = gst_context_writable_structure(gst_ctx);
            // TODO life-time of ContextPtr object
            for (const std::string &key : ctx->keys()) {
                gst_structure_set(s, key.c_str(), G_TYPE_POINTER, ctx->handle(key), NULL);
            }
            gst_query_set_context(query, gst_ctx);
            gst_context_unref(gst_ctx);

            GST_LOG_OBJECT(_base, "Created context of type %s", context_type);
            return TRUE;
        }
    }

    return _parent_class->query(_base, direction, query);
}

GstCaps *GstDlsTransform::transform_caps(GstPadDirection direction, GstCaps *caps, GstCaps *filter) {
    GST_DEBUG_OBJECT(_base, "transform_caps");

    // auto name = _base->element.object.name;
    // if (direction == GST_PAD_SRC)
    //    printf("%s: <<< OUT->INP %s\n", name, gst_caps_to_string(caps));
    // else
    //    printf("%s: <<< INP->OUT %s\n", name, gst_caps_to_string(caps));

    GstCaps *ret_caps = gst_caps_new_empty();
    for (guint i = 0; i < gst_caps_get_size(caps); i++) {
        gint framerate_n = 0, framerate_d = 0;
        gst_structure_get_fraction(gst_caps_get_structure(caps, i), "framerate", &framerate_n, &framerate_d);

        BufferInfoVector info2vector;
        if (_transform_base) {
            BufferInfo info1 = gst_caps_to_buffer_info(caps, i);

            if (direction == GST_PAD_SRC) {
                info2vector = _transform_base->get_input_info(info1);
            } else {
                info2vector = _transform_base->get_output_info(info1);
            }
            // Most inplace transforms return same caps
            if (info2vector.size() == 1 && info2vector[0] == info1) {
                gst_caps_append(ret_caps, gst_caps_copy(caps));
                continue;
            }
        } else {
            info2vector = direction == GST_PAD_SRC ? _class_data->desc->input_info : _class_data->desc->output_info;
        }

        if (!info2vector.empty()) {
            GstCaps *caps2 = buffer_info_vector_to_gst_caps(info2vector);
            if (framerate_n && framerate_d) {
                for (guint j = 0; j < gst_caps_get_size(caps2); j++) {
                    GstStructure *caps2_str = gst_caps_get_structure(caps2, j);
                    gst_structure_set(caps2_str, "framerate", GST_TYPE_FRACTION, framerate_n, framerate_d, NULL);
                }
            }
            // printf("%s: >>> %s\n", name, gst_caps_to_string(caps2));
            if (filter) {
                GstCaps *intersection = gst_caps_intersect_full(filter, caps2, GST_CAPS_INTERSECT_FIRST);
                gst_caps_unref(caps2);
                caps2 = intersection;
            }
            // printf("%s: >>> %s\n", name, gst_caps_to_string(caps2));
            if (!gst_caps_is_empty(caps2))
                gst_caps_append(ret_caps, caps2);
        }
    }
    return ret_caps;
}

gboolean GstDlsTransform::set_caps(GstCaps *incaps, GstCaps *outcaps) {
    GST_DEBUG_OBJECT(_base, "transform_caps");

    // auto name = _base->element.object.name;
    // printf("%s: !!!!!!!!!!!!!!! in  %s\n", name, gst_caps_to_string(incaps));
    // printf("%s: !!!!!!!!!!!!!!! out %s\n", name, gst_caps_to_string(outcaps));
    _input_info = gst_caps_to_buffer_info(incaps);
    _output_info = gst_caps_to_buffer_info(outcaps);
    try {
        ensure_transform_ready();
    } catch (std::exception &e) {
        GST_ERROR_OBJECT(_base, "Couldn't prepare transform instance for processing: %s", e.what());
    }
    return _transform_ready;
}

GstFlowReturn GstDlsTransform::generate_output(GstBuffer **outbuf) {
    ITT_TASK(trans->element.object.name);
    GST_DEBUG_OBJECT(_base, "generate_output");

    if (!_base->queued_buf)
        return GST_FLOW_OK;

    GstBuffer *input = _base->queued_buf;
    auto in = std::make_shared<GSTBuffer>(input, _input_info, true);
    _base->queued_buf = nullptr; // GSTBuffer took ownership

    if (_stream_id) { // Set stream_id field in SourceIdentifierMetadata
        auto source_id_meta = find_metadata(*in, SourceIdentifierMetadata::name);
        if (!source_id_meta)
            source_id_meta = in->add_metadata(SourceIdentifierMetadata::name);
        source_id_meta->set(SourceIdentifierMetadata::key::stream_id, _stream_id);
    }

    if (!_pool) {
        auto buffer_allocator = _transform_with_alloc->get_output_allocator();
        // TODO: can we use GstBufferPool instead?
        _pool.reset(new PoolSharedPtr<dlstreamer::BufferPtr>([buffer_allocator]() { return buffer_allocator(); },
                                                             _buffer_pool_size));
    }

    if (!_output_mapper)
        _output_mapper = _transform_with_alloc->get_output_mapper();

    BufferPtr out = _pool->get_or_create();
    // remove previous metadata
    for (auto meta : out->metadata()) {
        out->remove_metadata(meta);
    }

    bool ret = _transform_with_alloc->process(in, out);

    if (!ret) { // send gap event
        // auto gap_event = gst_event_new_gap(GST_BUFFER_PTS(input), GST_BUFFER_DURATION(input));
        // if (!gst_pad_push_event(_base->srcpad, gap_event))
        //     return GST_FLOW_ERROR;

        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    // Wrap dlstreamer::Buffer into GstBuffer
    *outbuf = buffer_to_gst_buffer(out, _output_mapper);

    // Copy timestamps and metadata
    if (!gst_buffer_copy_into(*outbuf, input, GST_BUFFER_COPY_METADATA, 0, static_cast<gsize>(-1)))
        throw std::runtime_error("Failed to copy GstBuffer info");

    if (_class_data->desc->flags & TRANSFORM_FLAG_MULTISTREAM_MUXER) {
        // All streams push data to first-registered stream
        GstBaseTransform *first_transform = g_gst_base_transform_storage.get_first(_transform_base.get());
        gst_pad_push(first_transform->srcpad, *outbuf);
        *outbuf = NULL;
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    return GST_FLOW_OK;
}

GstFlowReturn GstDlsTransform::transform(GstBuffer *inbuf, GstBuffer *outbuf) {
    ITT_TASK(trans->element.object.name);
    GST_DEBUG_OBJECT(_base, "transform");

    auto in = std::make_shared<GSTBuffer>(inbuf, _input_info, false);
    auto out = std::make_shared<GSTBuffer>(outbuf, _output_info, false);
    _transform->process(in, out);

    return GST_FLOW_OK;
}

GstFlowReturn GstDlsTransform::transform_ip(GstBuffer *buf) {
    ITT_TASK(trans->element.object.name);
    GST_DEBUG_OBJECT(_base, "transform_ip");

    auto in = std::make_shared<GSTBuffer>(buf, _input_info, false);

    // TODO: return value means if we should drop buffer (send GAP) or not
    // Redesign later by introducing another method to check if buffer should be dropped or by transform flag
    bool ret = _transform_inplace->process(in);

    if (!ret) {
        GST_DEBUG_OBJECT(_base, "Push GAP event: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
        auto event = gst_event_new_gap(GST_BUFFER_PTS(buf), GST_BUFFER_DURATION(buf));
        // If SourceIdentifierMetadata attached, copy all fields to GAP event
        auto source_id_meta = find_metadata(*in, SourceIdentifierMetadata::name);
        if (source_id_meta) {
            GSTDictionary event_dict(gst_event_writable_structure(event));
            copy_dictionary(*source_id_meta, event_dict);
        }
        // Push GAP event
        if (!gst_pad_push_event(_base->srcpad, event)) {
            GST_ERROR_OBJECT(_base, "Failed to push GAP event");
            return GST_FLOW_ERROR;
        }
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    return GST_FLOW_OK;
}

/////////////////////////////////////////////////////////////////////////////////////

void GstDlsTransformClass::init(gpointer g_class, gpointer class_data) {
    auto *self_class = static_cast<GstDlsTransformClass *>(g_class);
    auto *desc = static_cast<TransformDesc *>(class_data);
    self_class->desc = desc;

    self_class->private_offset = g_type_add_instance_private(G_TYPE_FROM_CLASS(g_class), sizeof(GstDlsTransform));
    g_type_class_adjust_private_offset(g_class, &self_class->private_offset);

    GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);
    // input caps
    GstCaps *input_caps = buffer_info_vector_to_gst_caps(desc->input_info);
    GstPadTemplate *sink_template = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, input_caps);
    gst_element_class_add_pad_template(element_class, sink_template);

    // output caps
    GstCaps *output_caps = buffer_info_vector_to_gst_caps(desc->output_info);
    GstPadTemplate *src_template = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, output_caps);
    gst_element_class_add_pad_template(element_class, src_template);

    // FIXME: The "Video" classification is not correct for all our use-cases
    gst_element_class_set_static_metadata(element_class, desc->name.data(), "Video", desc->description.data(),
                                          desc->author.data());

    GObjectClass *gobject_class = G_OBJECT_CLASS(g_class);
    gobject_class->finalize = GstDlsTransform::instance_finalize;
    gobject_class->set_property = Callback<&GstDlsTransform::set_property>::fn;
    gobject_class->get_property = Callback<&GstDlsTransform::get_property>::fn;

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(g_class);
    base_transform_class->start = Callback<&GstDlsTransform::start>::fn;
    base_transform_class->set_caps = Callback<&GstDlsTransform::set_caps>::fn;
    base_transform_class->transform_caps = Callback<&GstDlsTransform::transform_caps>::fn;
    base_transform_class->query = Callback<&GstDlsTransform::query>::fn;
    base_transform_class->transform_ip = Callback<&GstDlsTransform::transform_ip>::fn;
    base_transform_class->transform = Callback<&GstDlsTransform::transform>::fn;

    if (desc->flags & TRANSFORM_FLAG_OUTPUT_ALLOCATOR) {
        base_transform_class->generate_output = Callback<&GstDlsTransform::generate_output>::fn;
    }

    guint property_id = 0;
    constexpr auto param_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY);
    // Make reference to empty params if not present to avoid if statement and nesting
    const ParamDescVector &params = desc->params ? *desc->params : ParamDescVector{};
    for (auto const &param : params) {
        GParamSpec *pspec = std::visit(
            overloaded{[&](int v) {
                           return g_param_spec_int(param.name.c_str(), param.name.c_str(), param.description.c_str(),
                                                   AnyCast<int>(param.range[0]), AnyCast<int>(param.range[1]), v,
                                                   param_flags);
                       },
                       [&](double v) {
                           return g_param_spec_double(param.name.c_str(), param.name.c_str(), param.description.c_str(),
                                                      AnyCast<double>(param.range[0]), AnyCast<double>(param.range[1]),
                                                      v, param_flags);
                       },
                       [&](bool v) {
                           return g_param_spec_boolean(param.name.c_str(), param.name.c_str(),
                                                       param.description.c_str(), v, param_flags);
                       },
                       [&](const std::string &v) {
                           return g_param_spec_string(param.name.c_str(), param.name.c_str(), param.description.c_str(),
                                                      v.c_str(), param_flags);
                       },
                       [&](const intptr_t &) {
                           return g_param_spec_pointer(param.name.c_str(), param.name.c_str(),
                                                       param.description.c_str(), param_flags);
                       },
                       [](auto) { return static_cast<GParamSpec *>(nullptr); }},
            param.default_value);
        if (!pspec)
            throw std::runtime_error("Unsupported type of parameter: " + param.name);

        property_id++;
        g_object_class_install_property(gobject_class, property_id, pspec);
    }

    // Install additional parameters AFTER parameters from transform description
    if (desc->flags & TRANSFORM_FLAG_SHARABLE) {
        property_id++;
        g_object_class_install_property(
            gobject_class, property_id,
            g_param_spec_string(param::shared_instance_id, param::shared_instance_id,
                                "Identifier for sharing backend instance between multiple elements, for example in "
                                "elements processing multiple inputs",
                                "", param_flags));
    }
    if (desc->flags & TRANSFORM_FLAG_SUPPORT_PARAMS_STRUCTURE) {
        property_id++;
        g_object_class_install_property(gobject_class, property_id,
                                        g_param_spec_pointer(param::params_structure, param::params_structure,
                                                             "All parameters as GstStructure* pointer", param_flags));
    }
    if (desc->flags & TRANSFORM_FLAG_OUTPUT_ALLOCATOR) {
        property_id++;
        g_object_class_install_property(gobject_class, property_id,
                                        g_param_spec_int(param::buffer_pool_size, param::buffer_pool_size,
                                                         "Max size of output buffer pool", 0, INT32_MAX,
                                                         BUFFER_POOL_SIZE_DEFAULT, param_flags));
    }
}

///////////////////////////////////////////////////////////////////////////////////////

static const GTypeInfo gst_dls_transform_type_info = {.class_size = sizeof(GstDlsTransformClass),
                                                      .base_init = NULL,
                                                      .base_finalize = NULL,
                                                      .class_init = GstDlsTransformClass::init,
                                                      .class_finalize = NULL,
                                                      .class_data = NULL,
                                                      .instance_size = sizeof(GstDlsTransform::GstPodData),
                                                      .n_preallocs = 0,
                                                      .instance_init = GstDlsTransform::instance_init,
                                                      .value_table = NULL};

bool register_transform_as_gstreamer(GstPlugin *plugin, const dlstreamer::TransformDesc &desc) {
    GTypeInfo type_info = dlstreamer::gst_dls_transform_type_info;
    type_info.class_data = &desc;
    GType gtype = g_type_register_static(GST_TYPE_BASE_TRANSFORM, desc.name.data(), &type_info, (GTypeFlags)0);
    return gst_element_register(plugin, desc.name.data(), GST_RANK_NONE, gtype);
}

} // namespace dlstreamer
