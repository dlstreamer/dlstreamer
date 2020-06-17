/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <nlohmann/json.hpp>

const nlohmann::json GALLERY_SCHEMA = R"({
   "definitions": {},
   "$schema": "http://json-schema.org/draft-07/schema#",
   "$id": "http://example.com/root.json",
   "type": "array",
   "title": "The Root Schema",
   "items": {
     "$id": "#/items",
     "type": "object",
     "title": "The Items Schema",
     "required": [
       "name",
       "features"
     ],
     "properties": {
       "name": {
         "$id": "#/items/properties/name",
         "type": "string",
         "title": "The Name Schema",
         "default": "",
         "examples": [
           "Ivan"
         ],
         "pattern": "^(.*)$"
       },
       "features": {
         "$id": "#/items/properties/features",
         "type": "array",
         "title": "The Features Schema",
         "items": {
           "$id": "#/items/properties/features/items",
           "type": "string",
           "title": "The Items Schema",
           "default": "",
           "examples": [
             "features/person2_0.tensor",
             "features/person2_1.tensor"
           ],
           "pattern": "^(.*)$"
         }
       }
     }
   }
 }
)"_json;
