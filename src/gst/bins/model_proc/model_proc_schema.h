/*******************************************************************************
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <nlohmann/json.hpp>

const nlohmann::json MODEL_PROC_SCHEMA_V2_2 = R"({
  "definitions": {},
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "http://example.com/root.json",
  "type": "object",
  "title": "The Root Schema",
  "self": {
                "vendor": "Intel Corporation",
                "version": "2.2.0"
  },
  "required": [
    "json_schema_version",
    "input_preproc",
    "output_postproc"
  ],
  "properties": {
    "json_schema_version": {
      "$id": "#/properties/json_schema_version",
      "type": "string",
      "title": "The Json_schema_version Schema",
      "pattern": "^\\d{1,10}\\.\\d{1,10}\\.\\d{1,10}$",
      "default": "2.0.0",
      "examples": [
        "2.0.0"
      ]
    },
    "input_preproc": {
      "$id": "#/properties/input_preproc",
      "type": "array",
      "title": "The Input Model Preprocessing Schema",
      "items": {
        "$id": "#/properties/input_preproc/items",
        "type": "object",
        "title": "The Items Schema",
        "properties": {
          "layer_name": {
            "$id": "#/properties/input_preproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer Name Schema",
            "default": "ANY",
            "examples": [
              "0"
            ]
          },
          "format": {
            "$id": "#/properties/input_preproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "image",
            "examples": [
              "image"
            ]
          },
          "params": {
            "$id": "#/properties/input_preproc/items/properties/params",
            "type": "object",
            "title": "The Params Schema"
          }
        }
      }
    },
    "output_postproc": {
      "$id": "#/properties/output_postproc",
      "type": "array",
      "title": "The Output_postproc Schema",
      "items": {
        "$id": "#/properties/output_postproc/items",
        "type": "object",
        "title": "The Items Schema",
        "properties": {
          "layer_name": {
            "$id": "#/properties/output_postproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer_name Schema",
            "default": "",
            "examples": [
              "658"
            ]
          },
          "layer_names": {
            "$id": "#/properties/output_postproc/items/properties/layer_names",
            "type": "array",
            "title": "The Layer_names Schema",
            "items": {
              "$id": "#/properties/input_preproc/items/properties/alignment_points/items",
              "type": "string",
              "title": "The Items Schema",
              "default": "",
              "examples": [
                "layer_1",
                "layer_2"
              ]
            }
          },
          "attribute_name": {
            "$id": "#/properties/output_postproc/items/properties/attribute_name",
            "type": "string",
            "title": "The Attribute_name Schema",
            "default": "",
            "examples": [
              "face_id"
            ]
          },
          "format": {
            "$id": "#/properties/output_postproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "",
            "examples": [
              "cosine_distance"
            ]
          },
          "labels": {
            "$id": "#/properties/output_postproc/items/properties/labels",
            "type": ["array", "string"],
            "title": "The Labels Schema",
            "items": {
              "$id": "#/properties/output_postproc/items/properties/labels/items",
              "type": ["string", "object"],
              "title": "The Items Schema",
              "default": "",
              "examples": [
                "dog",
                "cat"
              ]
            },
            "examples": [
              ["dog", "cat"],
              "path"
            ]
          }
        }
      }
    }
  }
})"_json;

const nlohmann::json MODEL_PROC_SCHEMA_V2_1 = R"({
  "definitions": {},
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "http://example.com/root.json",
  "type": "object",
  "title": "The Root Schema",
  "self": {
                "vendor": "Intel Corporation",
                "version": "2.1.0"
  },
  "required": [
    "json_schema_version",
    "input_preproc",
    "output_postproc"
  ],
  "properties": {
    "json_schema_version": {
      "$id": "#/properties/json_schema_version",
      "type": "string",
      "title": "The Json_schema_version Schema",
      "pattern": "^\\d{1,10}\\.\\d{1,10}\\.\\d{1,10}$",
      "default": "2.0.0",
      "examples": [
        "2.0.0"
      ]
    },
    "input_preproc": {
      "$id": "#/properties/input_preproc",
      "type": "array",
      "title": "The Input Model Preprocessing Schema",
      "items": {
        "$id": "#/properties/input_preproc/items",
        "type": "object",
        "title": "The Items Schema",
        "required": [
          "layer_name",
          "format"
        ],
        "properties": {
          "layer_name": {
            "$id": "#/properties/input_preproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer Name Schema",
            "default": "",
            "examples": [
              "0"
            ]
          },
          "format": {
            "$id": "#/properties/input_preproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "",
            "examples": [
              "image"
            ]
          },
          "params": {
            "$id": "#/properties/input_preproc/items/properties/params",
            "type": "object",
            "title": "The Params Schema"
          }
        }
      }
    },
    "output_postproc": {
      "$id": "#/properties/output_postproc",
      "type": "array",
      "title": "The Output_postproc Schema",
      "items": {
        "$id": "#/properties/output_postproc/items",
        "type": "object",
        "title": "The Items Schema",
        "properties": {
          "layer_name": {
            "$id": "#/properties/output_postproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer_name Schema",
            "default": "",
            "examples": [
              "658"
            ]
          },
          "layer_names": {
            "$id": "#/properties/output_postproc/items/properties/layer_names",
            "type": "array",
            "title": "The Layer_names Schema",
            "items": {
              "$id": "#/properties/input_preproc/items/properties/alignment_points/items",
              "type": "string",
              "title": "The Items Schema",
              "default": "",
              "examples": [
                "layer_1",
                "layer_2"
              ]
            }
          },
          "attribute_name": {
            "$id": "#/properties/output_postproc/items/properties/attribute_name",
            "type": "string",
            "title": "The Attribute_name Schema",
            "default": "",
            "examples": [
              "face_id"
            ]
          },
          "format": {
            "$id": "#/properties/output_postproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "",
            "examples": [
              "cosine_distance"
            ]
          }
        }
      }
    }
  }
})"_json;

const nlohmann::json MODEL_PROC_SCHEMA_V2 = R"({
  "definitions": {},
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "http://example.com/root.json",
  "type": "object",
  "title": "The Root Schema",
  "self": {
                "vendor": "Intel Corporation",
                "version": "2.0.0"
  },
  "required": [
    "json_schema_version",
    "input_preproc",
    "output_postproc"
  ],
  "properties": {
    "json_schema_version": {
      "$id": "#/properties/json_schema_version",
      "type": "string",
      "title": "The Json_schema_version Schema",
      "pattern": "^\\d{1,10}\\.\\d{1,10}\\.\\d{1,10}$",
      "default": "2.0.0",
      "examples": [
        "2.0.0"
      ]
    },
    "input_preproc": {
      "$id": "#/properties/input_preproc",
      "type": "array",
      "title": "The Input Model Preprocessing Schema",
      "items": {
        "$id": "#/properties/input_preproc/items",
        "type": "object",
        "title": "The Items Schema",
        "required": [
          "layer_name",
          "format"
        ],
        "properties": {
          "layer_name": {
            "$id": "#/properties/input_preproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer Name Schema",
            "default": "",
            "examples": [
              "0"
            ]
          },
          "format": {
            "$id": "#/properties/input_preproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "",
            "examples": [
              "image"
            ]
          },
          "params": {
            "$id": "#/properties/input_preproc/items/properties/params",
            "type": "object",
            "title": "The Params Schema"
          }
        }
      }
    },
    "output_postproc": {
      "$id": "#/properties/output_postproc",
      "type": "array",
      "title": "The Output_postproc Schema",
      "items": {
        "$id": "#/properties/output_postproc/items",
        "type": "object",
        "title": "The Items Schema",
        "properties": {
          "layer_name": {
            "$id": "#/properties/output_postproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer_name Schema",
            "default": "",
            "examples": [
              "658"
            ]
          },
          "attribute_name": {
            "$id": "#/properties/output_postproc/items/properties/attribute_name",
            "type": "string",
            "title": "The Attribute_name Schema",
            "default": "",
            "examples": [
              "face_id"
            ]
          },
          "format": {
            "$id": "#/properties/output_postproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "",
            "examples": [
              "cosine_distance"
            ]
          }
        }
      }
    }
  }
})"_json;

const nlohmann::json MODEL_PROC_SCHEMA_V1 = R"({
  "definitions": {},
  "$schema": "http://json-schema.org/draft-07/schema#",
  "$id": "http://example.com/root.json",
  "type": "object",
  "title": "The Root Schema",
  "self": {
                "vendor": "Intel Corporation",
                "version": "1.0.0"
  },
  "required": [
    "json_schema_version",
    "input_preproc",
    "output_postproc"
  ],
  "properties": {
    "json_schema_version": {
      "$id": "#/properties/json_schema_version",
      "type": "string",
      "title": "The Json_schema_version Schema",
      "pattern": "^\\d{1,10}\\.\\d{1,10}\\.\\d{1,10}$",
      "default": "1.0.0",
      "examples": [
        "1.0.0"
      ]
    },
    "input_preproc": {
      "$id": "#/properties/input_preproc",
      "type": "array",
      "title": "The Input_preproc Schema",
      "items": {
        "$id": "#/properties/input_preproc/items",
        "type": "object",
        "title": "The Items Schema",
        "properties": {
          "layer_name": {
            "$id": "#/properties/input_preproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer_name Schema",
            "default": "",
            "examples": [
              "0"
            ]
          },
          "color_format": {
            "$id": "#/properties/input_preproc/items/properties/color_format",
            "type": "string",
            "title": "The Color_format Schema",
            "default": "",
            "examples": [
              "BGR"
            ]
          },
          "converter": {
            "$id": "#/properties/input_preproc/items/properties/converter",
            "type": "string",
            "title": "The Converter Schema",
            "default": "",
            "examples": [
              "alignment"
            ]
          },
          "alignment_points": {
            "$id": "#/properties/input_preproc/items/properties/alignment_points",
            "type": "array",
            "title": "The Alignment_points Schema",
            "items": {
              "$id": "#/properties/input_preproc/items/properties/alignment_points/items",
              "type": "number",
              "title": "The Items Schema",
              "default": 0.0,
              "examples": [
                0.31556875,
                0.4615741071428571,
                0.6826229166666667,
                0.4615741071428571,
                0.5002624999999999,
                0.6405053571428571,
                0.34947187500000004,
                0.8246919642857142,
                0.6534364583333333,
                0.8246919642857142
              ]
            }
          }
        }
      }
    },
    "output_postproc": {
      "$id": "#/properties/output_postproc",
      "type": "array",
      "title": "The Output_postproc Schema",
      "items": {
        "$id": "#/properties/output_postproc/items",
        "type": "object",
        "title": "The Items Schema",
        "properties": {
          "layer_name": {
            "$id": "#/properties/output_postproc/items/properties/layer_name",
            "type": "string",
            "title": "The Layer_name Schema",
            "default": "",
            "examples": [
              "658"
            ]
          },
          "attribute_name": {
            "$id": "#/properties/output_postproc/items/properties/attribute_name",
            "type": "string",
            "title": "The Attribute_name Schema",
            "default": "",
            "examples": [
              "face_id"
            ]
          },
          "format": {
            "$id": "#/properties/output_postproc/items/properties/format",
            "type": "string",
            "title": "The Format Schema",
            "default": "",
            "examples": [
              "cosine_distance"
            ]
          }
        }
      }
    }
  }
})"_json;
