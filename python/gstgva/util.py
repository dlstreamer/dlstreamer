# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import ctypes
from contextlib import contextmanager

# libgstreamer
libgst = ctypes.CDLL("libgstreamer-1.0.so.0")

GST_PADDING = 4


class GstMapInfo(ctypes.Structure):
    _fields_ = [("memory", ctypes.c_void_p),  # GstMemory *memory
                ("flags", ctypes.c_int),  # GstMapFlags flags
                ("data", ctypes.POINTER(ctypes.c_byte)),  # guint8 *data
                ("size", ctypes.c_size_t),  # gsize size
                ("maxsize", ctypes.c_size_t),  # gsize maxsize
                ("user_data", ctypes.c_void_p * 4),  # gpointer user_data[4]
                ("_gst_reserved", ctypes.c_void_p * GST_PADDING)]


GST_MAP_INFO_POINTER = ctypes.POINTER(GstMapInfo)


class GUnion(ctypes.Union):
    _fields_ = [('v_int', ctypes.c_int),
                ('v_uint', ctypes.c_uint),
                ('v_long', ctypes.c_long),
                ('v_ulong', ctypes.c_ulong),
                ('v_int64', ctypes.c_int64),
                ('v_uint64', ctypes.c_uint64),
                ('v_float', ctypes.c_float),
                ('v_double', ctypes.c_double),
                ('v_pointer', ctypes.c_void_p)]


class GValue(ctypes.Structure):
    _fields_ = [('g_type', ctypes.c_size_t),
                ('data', GUnion)]


G_VALUE_POINTER = ctypes.POINTER(GValue)


class GValueArray(ctypes.Structure):
    _fields_ = [("n_values", ctypes.c_uint32),
                ("values", ctypes.c_void_p),
                ("n_preallocated", ctypes.c_uint32)]


G_VALUE_ARRAY_POINTER = ctypes.POINTER(GValueArray)

# gst buffer
libgst.gst_buffer_map.argtypes = [
    ctypes.c_void_p, GST_MAP_INFO_POINTER, ctypes.c_int]
libgst.gst_buffer_map.restype = ctypes.c_int
libgst.gst_buffer_unmap.argtypes = [ctypes.c_void_p, GST_MAP_INFO_POINTER]
libgst.gst_buffer_unmap.restype = None
libgst.gst_buffer_iterate_meta_filtered.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p]
libgst.gst_buffer_iterate_meta_filtered.restype = ctypes.c_void_p
libgst.gst_buffer_remove_meta.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
libgst.gst_buffer_remove_meta.restype = ctypes.c_bool
libgst.gst_buffer_add_meta.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
libgst.gst_buffer_add_meta.restype = ctypes.c_void_p

# gst miniobject
libgst.gst_mini_object_make_writable.argtypes = [ctypes.c_void_p]
libgst.gst_mini_object_make_writable.restype = ctypes.c_void_p
libgst.gst_mini_object_is_writable.argtypes = [ctypes.c_void_p]
libgst.gst_mini_object_is_writable.restype = ctypes.c_int
libgst.gst_mini_object_ref.argtypes = [ctypes.c_void_p]
libgst.gst_mini_object_ref.restype = ctypes.c_void_p
libgst.gst_mini_object_unref.argtypes = [ctypes.c_void_p]
libgst.gst_mini_object_unref.restype = ctypes.c_void_p

# gst structure
libgst.gst_structure_get_name.argtypes = [ctypes.c_void_p]
libgst.gst_structure_get_name.restype = ctypes.c_char_p
libgst.gst_structure_has_name.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
libgst.gst_structure_has_name.restype = ctypes.c_bool
libgst.gst_structure_set_name.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
libgst.gst_structure_set_name.restypes = None
libgst.gst_structure_set_value.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]
libgst.gst_structure_set_value.restypes = None
libgst.gst_structure_set_array.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]
libgst.gst_structure_set_array.restypes = None
libgst.gst_structure_remove_field.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
libgst.gst_structure_remove_field.restypes = None
libgst.gst_structure_get_field_type.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p]
libgst.gst_structure_get_field_type.restypes = ctypes.c_size_t
libgst.gst_structure_get_string.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
libgst.gst_structure_get_string.restype = ctypes.c_char_p
libgst.gst_structure_get_value.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
libgst.gst_structure_get_value.restype = ctypes.c_void_p
libgst.gst_structure_get_int.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
libgst.gst_structure_get_int.restype = ctypes.c_int
libgst.gst_structure_get_double.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)]
libgst.gst_structure_get_double.restype = ctypes.c_int
libgst.gst_structure_get_array.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(G_VALUE_ARRAY_POINTER)]
libgst.gst_structure_get_array.restype = ctypes.c_bool
libgst.gst_structure_n_fields.argtypes = [ctypes.c_void_p]
libgst.gst_structure_n_fields.restype = ctypes.c_int
libgst.gst_structure_nth_field_name.argtypes = [ctypes.c_void_p, ctypes.c_uint]
libgst.gst_structure_nth_field_name.restype = ctypes.c_char_p
libgst.gst_structure_new_empty.argtypes = [ctypes.c_char_p]
libgst.gst_structure_new_empty.restype = ctypes.c_void_p
libgst.gst_structure_copy.argtypes = [ctypes.c_void_p]
libgst.gst_structure_copy.restype = ctypes.c_void_p

# gst caps
libgst.gst_caps_get_structure.argtypes = [ctypes.c_void_p, ctypes.c_uint]
libgst.gst_caps_get_structure.restype = ctypes.c_void_p

libgst.gst_value_array_get_type.argtypes = None
libgst.gst_value_array_get_type.restype = ctypes.c_void_p

libgst.gst_value_array_append_value.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
libgst.gst_value_array_append_value.restype = None

@contextmanager
def GST_PAD_PROBE_INFO_BUFFER(info):
    _buffer = info.get_buffer()
    _buffer.mini_object.refcount -= 1
    try:
        yield _buffer
    finally:
        _buffer.mini_object.refcount += 1


@contextmanager
def TRANSFORM_IP_BUFFER(_buffer):
    _buffer.mini_object.refcount -= 1
    try:
        yield _buffer
    finally:
        _buffer.mini_object.refcount += 1


@contextmanager
def gst_buffer_data(_buffer, flags):
    if _buffer is None:
        raise TypeError("Cannot pass NULL to gst_buffer_map")

    ptr = hash(_buffer)

    mapping = GstMapInfo()
    success = libgst.gst_buffer_map(ptr, mapping, flags)

    if not success:
        raise RuntimeError("Couldn't map buffer")

    try:
        yield ctypes.cast(mapping.data, ctypes.POINTER(ctypes.c_byte * mapping.size)).contents
    finally:
        libgst.gst_buffer_unmap(ptr, mapping)


# libgobject
libgobject = ctypes.CDLL('libgobject-2.0.so')


class GList(ctypes.Structure):
    pass


GLIST_POINTER = ctypes.POINTER(GList)

GList._fields_ = [
    ('data', ctypes.c_void_p),
    ('next', GLIST_POINTER),
    ('prev', GLIST_POINTER)
]

libgobject.g_type_from_name.argtypes = [ctypes.c_char_p]
libgobject.g_type_from_name.restype = ctypes.c_ulong
libgobject.g_value_get_variant.argtypes = [ctypes.c_void_p]
libgobject.g_value_get_variant.restype = ctypes.c_void_p
libgobject.g_value_get_int.argtypes = [ctypes.c_void_p]
libgobject.g_value_get_int.restype = ctypes.c_void_p
libgobject.g_value_set_variant.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
libgobject.g_value_set_variant.restype = None
libgobject.g_variant_get_fixed_array.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t]
libgobject.g_variant_get_fixed_array.restype = ctypes.c_void_p
libgobject.g_list_remove.argtypes = [GLIST_POINTER, ctypes.c_void_p]
libgobject.g_list_remove.restypes = GLIST_POINTER
libgobject.g_variant_new_fixed_array.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t]
libgobject.g_variant_new_fixed_array.restype = ctypes.c_void_p
libgobject.g_value_array_new.argtypes = [ctypes.c_size_t]
libgobject.g_value_array_new.restype = G_VALUE_ARRAY_POINTER
libgobject.g_value_init.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
libgobject.g_value_init.restype = ctypes.c_void_p
libgobject.g_value_set_uint.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
libgobject.g_value_set_uint.restype = ctypes.c_void_p
libgobject.g_value_set_int.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
libgobject.g_value_set_int.restype = ctypes.c_void_p
libgobject.g_value_array_append.argtypes = [G_VALUE_ARRAY_POINTER,  ctypes.c_void_p]
libgobject.g_value_array_append.restype = G_VALUE_ARRAY_POINTER
libgobject.g_value_array_get_nth.argtypes = [G_VALUE_ARRAY_POINTER, ctypes.c_uint]
libgobject.g_value_array_get_nth.restype = G_VALUE_POINTER
libgobject.g_value_get_uint.argtypes = [G_VALUE_POINTER]
libgobject.g_value_get_uint.restype = ctypes.c_uint

# libgstvideo
libgstvideo = ctypes.CDLL("libgstvideo-1.0.so")

###
libgstva = ctypes.CDLL("libgstvideoanalyticsmeta.so")
