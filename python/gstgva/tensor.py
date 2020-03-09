# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

## @file tensor.py
#  @brief This file contains gstgva.tensor.Tensor class which contains and describes neural network inference result

import ctypes
import numpy
import gi
from typing import List

gi.require_version('Gst', '1.0')

from enum import Enum
from gi.repository import GObject, Gst
from .util import libgst, libgobject, G_VALUE_ARRAY_POINTER, GValueArray, GValue, G_VALUE_POINTER

GVA_TENSOR_MAX_RANK = 8

## @brief This class represents tensor - map-like storage for inference result information, such as output blob
# description (output layer dims, layout, rank, precision, etc.), inference result in a raw and interpreted forms.
# Tensor is based on GstStructure and, in general, can contain arbitrary (user-defined) fields of simplest data types,
# like integers, floats & strings.
# Tensor can contain only raw inference result (such Tensor is produced by gvainference in Gstreamer pipeline),
# detection result (such Tensor is produced by gvadetect in Gstreamer pipeline and it's called detection Tensor), or
# both raw & interpreted inference results (such Tensor is produced by gvaclassify in Gstreamer pipeline).
# Tensors can be created and used on their own, or they can be created within RegionOfInterest or VideoFrame instances.
# Usually, in Gstreamer pipeline with GVA elements (gvadetect, gvainference, gvaclassify) Tensor objects will be
# available for access and modification from RegionOfInterest and VideoFrame instances
class Tensor:
    # TODO: find a way to get these enums from C/C++ code and avoid duplicating

    ## @brief This enum describes model layer precision
    class PRECISION(Enum):
        ANY = 0    
        FP32 = 10  
        U8 = 40     

    ## @brief This enum describes model layer layout
    class LAYOUT(Enum):
        ANY = 0
        NCHW = 1
        NHWC = 2
        NC = 193
   
    ## @brief Construct Tensor instance from C-style GstStructure
    #  @param structure C-style pointer to GstStructure to create Tensor instance from.
    # There are much simpler ways for creating and obtaining Tensor instances - see RegionOfInterest and VideoFrame classes
    def __init__(self, structure: ctypes.c_void_p):
        self.__structure = structure
        if not self.__structure:
            raise ValueError("Tensor: structure passed is nullptr")

    ## @brief Set item to Tensor. It can be one of the following types: string, int, float.
    #  @param key Name of new field
    #  @param item Item
    def __setitem__(self, key: str, item) -> None:
        gvalue = GObject.Value()
        if type(item) is str:
            gvalue.init(GObject.TYPE_STRING)
            gvalue.set_string(item)
        elif type(item) is int:
            gvalue.init(GObject.TYPE_INT)
            gvalue.set_int(item)
        elif type(item) is float:
            gvalue.init(GObject.TYPE_DOUBLE)
            gvalue.set_double(item)
        elif type(item) is list:
            # code below doesn't work though it's very similar to C code used in GVA which works
            # gvalue_array = GObject.Value()
            # libgobject.g_value_init(hash(gvalue), ctypes.c_size_t(24))  # 24 is G_TYPE_INT
            # libgobject.g_value_init(hash(gvalue_array), libgst.gst_value_array_get_type())
            # for i in item:
            #     libgobject.g_value_set_int(hash(gvalue),i)
            #     libgst.gst_value_array_append_value(hash(gvalue_array),hash(gvalue))
            # libgst.gst_structure_set_value(self.__structure, key.encode('utf-8'), hash(gvalue_array))
            raise NotImplementedError
        else:
            raise TypeError
        libgst.gst_structure_set_value(
            self.__structure, key.encode('utf-8'), hash(gvalue))

    ## @brief Get item by the field name 
    #  @param key Field name
    #  @return Item, None if failed to get
    def __getitem__(self, key):
        key = key.encode('utf-8')
        gtype = libgst.gst_structure_get_field_type(self.__structure, key)
        if gtype == hash(GObject.TYPE_INVALID):  # key is not found
            return None
        elif gtype == hash(GObject.TYPE_STRING):
            res = libgst.gst_structure_get_string(self.__structure, key)
            return res.decode("utf-8") if res else None
        elif gtype == hash(GObject.TYPE_INT):
            value = ctypes.c_int()
            res = libgst.gst_structure_get_int(
                self.__structure, key, ctypes.byref(value))
            return value.value if res else None
        elif gtype == hash(GObject.TYPE_DOUBLE):
            value = ctypes.c_double()
            res = libgst.gst_structure_get_double(
                self.__structure, key, ctypes.byref(value))
            return value.value if res else None
        else:
            # try to get value as GValueArray (e.g., "dims" key)
            value = list()
            gvalue_array = G_VALUE_ARRAY_POINTER()
            is_array = libgst.gst_structure_get_array(self.__structure, key, ctypes.byref(gvalue_array))
            if not is_array:
                raise TypeError("Tensor can contain only str, int, float or array")
            else:
                for i in range(0, gvalue_array.contents.n_values):
                    g_value = libgobject.g_value_array_get_nth(gvalue_array, ctypes.c_uint(i))
                    try:
                        value.append(libgobject.g_value_get_uint(g_value))
                    except Exception:
                        raise TypeError("Tensor array can contain only uint values")        
                return value

    ## @brief Get number of fields contained in Tensor instance
    #  @return Number of fields contained in Tensor instance
    def __len__(self) -> int:
        return libgst.gst_structure_n_fields(self.__structure)
    
    ## @brief Iterable by all Tensor fields
    # @return Generator for all Tensor fields
    def __iter__(self):
        for key in self.fields():
            yield key, self.__getitem__(key)

    ## @brief Return string represenation of the Tensor instance
    #  @return String of field names and values
    def __repr__(self) -> str:
        return repr(dict(self))

    ## @brief Remove item by the field name
    #  @param key Field name
    def __delitem__(self, key: str) -> None:
        libgst.gst_structure_remove_field(self.__structure, key.encode('utf-8'))
    
    ## @brief Get list of fields contained in Tensor instance
    #  @return List of fields contained in Tensor instance
    def fields(self) -> List[str]:
        return [libgst.gst_structure_nth_field_name(self.__structure, i).decode("utf-8") for i in range(self.__len__())]

    ## @brief Get label. This label is set for Tensor instances produced by gvaclassify element. It will raise exception 
    # if called for detection Tensor. To get detection class label, use RegionOfInterest.label
    #  @return label as a string, None if failed to get
    def label(self) -> str:
        if not self.is_detection():
            return self["label"]
        else:
            raise RuntimeError("Detection GVA::Tensor cann't have label.")

    ## @brief Get label id
    #  @return label id as an int, None if failed to get
    def label_id(self) -> int:
        return self["label_id"]

    ## @brief Get object id
    #  @return object id as an int, None if failed to get
    def object_id(self) -> int:
        return self["object_id"]

    ## @brief Get model name which was used for inference
    #  @return model name as a string, None if failed to get
    def model_name(self) -> str:
        return self["model_name"]

    ## @brief Get format
    #  @return format as a string, None if failed to get
    def format(self) -> str:
        return self["format"]

    ## @brief Get confidence of inference result
    #  @return confidence of inference result as a float, None if failed to get
    def confidence(self) -> float:
        return self["confidence"]
    
    ## @brief Get inference result blob layer name
    #  @return layer name as a string, None if failed to get
    def layer_name(self) -> str:
        return self["layer_name"]

    ## @brief Get number of inference result blob dimensions
    #  @return number of inference result blob dimensions, None if failed to get
    def rank(self) -> int:
        return self["rank"]

    ## @brief Get inference-id property value of GVA element from which this Tensor came
    #  @return inference-id property value of GVA element from which this Tensor came, None if failed to get
    def element_id(self) -> str:
        return self["element_id"]

    ## @brief Get inference result blob size in bytes
    #  @return inference result blob size in bytes, None if failed to get
    def total_bytes(self) -> int:
        return self["total_bytes"]

    ## @brief Get name as a string
    #  @return Tensor instance's name
    def name(self) -> str:
        name = libgst.gst_structure_get_name(self.__structure)
        if name:
            return name.decode('utf-8')
        return None
     
    ## @brief Set Tensor instance's name 
    def set_name(self, name: str) -> None:
        libgst.gst_structure_set_name(self.__structure, name.encode('utf-8'))
    
    ## @brief Get raw inference result blob data
    #  @return numpy.ndarray of values representing raw inference data, None if data can't be read
    def data(self) -> numpy.ndarray:
        precision = self.precision()
        if precision == self.PRECISION.FP32:
            view = numpy.float32
        elif precision == self.PRECISION.U8:
            view = numpy.uint8
        else:
            return None
        gvalue = libgst.gst_structure_get_value(
            self.__structure, 'data_buffer'.encode('utf-8'))
        if gvalue:
            gvariant = libgobject.g_value_get_variant(gvalue)
            nbytes = ctypes.c_size_t()
            data_ptr = libgobject.g_variant_get_fixed_array(
                gvariant, ctypes.byref(nbytes), 1)
            array_type = ctypes.c_ubyte * nbytes.value
            return numpy.ctypeslib.as_array(array_type.from_address(data_ptr)).view(dtype=view)
        return None
    
    ## @brief Get inference result blob layout
    #  @return LAYOUT, LAYOUT.ANY if can't be read
    def layout(self) -> LAYOUT:
        try:
            return self.LAYOUT(self["layout"])
        except:
            return self.LAYOUT.ANY

    ## @brief Get inference result blob layout as a string
    #  @return layout as a string, "ANY" if can't be read
    def layout_as_string(self) -> str:
        # TODO: share these strings with C/C++ code and avoid duplicating
        layout = self.layout()
        if layout == self.LAYOUT.NCHW:
            return "NCHW"
        elif layout == self.LAYOUT.NHWC:
            return "NHWC"
        elif layout == self.LAYOUT.NC:
            return "NC"
        else:
            return "ANY"

    ## @brief Get inference result blob dimensions info
    #  @return list of dimensions
    def dims(self) -> List[int]:  
        return self["dims"]
    
    ## @brief Get inference results blob precision
    #  @return PRECISION, PRECISION.ANY if can't be read
    def precision(self) -> PRECISION:
        try:
            return self.PRECISION(self["precision"])
        except:
            return self.PRECISION.ANY

    ## @brief Get inference results blob precision as a string
    #  @return precision as a string, "UNSPECIFIED" if can't be read
    def precision_as_string(self) -> str:
        # TODO: share these strings with C/C++ code and avoid duplicating
        precision = self.precision()
        if precision == self.PRECISION.U8:
            return "U8"
        elif precision == self.PRECISION.FP32:
            return "FP32"
        else:
            return "UNSPECIFIED"

    ## @brief Set label. It will raise exception if called for detection Tensor
    #  @param label label name as a string
    def set_label(self, label: str) -> None:
        if not self.is_detection():
            self['label'] = label
        else:
            raise RuntimeError("Detection GVA::Tensor can't have label.")
    
    ## @brief Check if Tensor instance has field
    #  @param field_name field name
    #  @return True if field with this name is found, False otherwise
    def has_field(self, field_name: str) -> bool:
        return True if self[field_name] else False
    
    ## @brief Check if this Tensor is detection Tensor (contains detection results)
    #  @return True if tensor contains detection results, False otherwise
    def is_detection(self) -> bool:
        return self.name() == "detection"