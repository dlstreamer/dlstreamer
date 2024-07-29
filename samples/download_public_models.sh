#!/bin/bash
# ==============================================================================
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -euo pipefail

MODEL=${1:-"all"} # Supported values listed in SUPPORTED_MODELS below.

SUPPORTED_MODELS=(
  "all"
  "yolo_all"
  "yolox-tiny"
  "yolox_s"
  "yolov5s"
  "yolov5su"
  "yolov7"
  "yolov8s"
  "yolov9c"
  "yolov8n-obb"
  "centerface"
  "hsemotion"
)

if ! [[ "${SUPPORTED_MODELS[*]}" =~ $MODEL ]]; then
  echo "Unsupported model: $MODEL" >&2
  exit 1
fi

set +u  # Disable nounset option
if [ -z "$MODELS_PATH" ]; then
  echo "MODELS_PATH is not specified"
  echo "Please set MODELS_PATH env variable with target path to download models"
  exit 1
fi
set -u  # Re-enable nounset option

echo Downloading models to folder "$MODELS_PATH"

# check if model exists in local directory, download as needed
if [ "$MODEL" == "yolox-tiny" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolox-tiny"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    mkdir -p "$MODELS_PATH"
    cd "$MODELS_PATH"
    echo "Downloading and converting: ${MODEL_PATH}"
    omz_downloader --name "$MODEL_NAME"
    omz_converter --name "$MODEL_NAME"
  fi
fi

if [ "$MODEL" == "yolox_s" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolox_s"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Model not found: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    wget https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s.onnx
    ovc yolox_s.onnx --compress_to_fp16=False
    rm -rf yolox_s
    cd ..
  fi
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP16/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Model not found: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    wget https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s.onnx
    ovc yolox_s.onnx
    rm -rf yolox_s
    cd ..
  fi
fi

if [ "$MODEL" == "yolov5su" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolov5su"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODELS_PATH"/public/"$MODEL_NAME"
    cd "$MODELS_PATH"/public/"$MODEL_NAME"
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov5s.pt")
model.info()
model.export(format='openvino', dynamic=True)  # creates 'yolov5su_openvino_model/'
from openvino.runtime import Core
from openvino.runtime import save_model
core = Core()
model = core.read_model("yolov5su_openvino_model/yolov5su.xml")
model.reshape([-1, 3, 640, 640])
save_model(model, "yolov5su_openvino_model/yolov5suD.xml")
EOF
    mkdir FP32
    mv yolov5su_openvino_model/yolov5suD.xml FP32/yolov5su.xml
    mv yolov5su_openvino_model/yolov5suD.bin FP32/yolov5su.bin
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov5s.pt")
model.info()
model.export(format='openvino', half=True, dynamic=True)  # creates 'yolov5su_openvino_model/'
from openvino.runtime import Core
from openvino.runtime import save_model
core = Core()
model = core.read_model("yolov5su_openvino_model/yolov5su.xml")
model.reshape([-1, 3, 640, 640])
save_model(model, "yolov5su_openvino_model/yolov5suD.xml")
EOF
    mkdir FP16
    mv yolov5su_openvino_model/yolov5suD.xml FP16/yolov5su.xml
    mv yolov5su_openvino_model/yolov5suD.bin FP16/yolov5su.bin
    rm -rf yolov5su_openvino_model
    cd ../..
  fi
fi

# Model yolov5s with FP32 precision only
if [ "$MODEL" == "yolov5s" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolov5s"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$MODELS_PATH
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/ultralytics/yolov5
    cd yolov5
    wget https://github.com/ultralytics/yolov5/releases/download/v7.0/yolov5s.pt

    python3 export.py --weights yolov5s.pt --include openvino --dynamic

    python3 - <<EOF
from openvino.runtime import Core
from openvino.runtime import save_model
core = Core()
model = core.read_model("yolov5s_openvino_model/yolov5s.xml")
model.reshape([-1, 3, 640, 640])
save_model(model, "yolov5s_openvino_model/yolov5sD.xml")
EOF
 
    mv yolov5s_openvino_model/yolov5sD.xml "$MODEL_DIR"/yolov5s.xml
    mv yolov5s_openvino_model/yolov5sD.bin "$MODEL_DIR"/yolov5s.bin
    cd ..
    rm -rf yolov5
    cd "$PREV_DIR"
  fi
fi

if [ "$MODEL" == "yolov7" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolov7"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP16/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$MODELS_PATH
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/WongKinYiu/yolov7.git
    cd yolov7
    python3 export.py --weights  yolov7.pt  --grid --dynamic-batch
    ovc yolov7.onnx
    mv yolov7.xml "$MODEL_PATH"
    mv yolov7.bin "$MODEL_DIR"
    cd ..
    rm -rf yolov7
    cd "$PREV_DIR"
  fi
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$MODELS_PATH
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/WongKinYiu/yolov7.git
    cd yolov7
    python3 export.py --weights  yolov7.pt  --grid --dynamic-batch
    ovc yolov7.onnx --compress_to_fp16=False
    mv yolov7.xml "$MODEL_PATH"
    mv yolov7.bin "$MODEL_DIR"
    cd ..
    rm -rf yolov7
    cd "$PREV_DIR"
  fi
fi

if [ "$MODEL" == "yolov8s" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolov8s"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODELS_PATH"/public/"$MODEL_NAME"
    cd "$MODELS_PATH"/public/"$MODEL_NAME"
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov8s.pt")
model.info()
model.export(format='openvino')  # creates 'yolov8s_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP32
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov8s.pt")
model.info()
model.export(format='openvino', half=True)  # creates 'yolov8s_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP16
    cd ../..
  fi
fi

if [ "$MODEL" == "yolov8n-obb" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolov8n-obb"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODELS_PATH"/public/"$MODEL_NAME"
    cd "$MODELS_PATH"/public/"$MODEL_NAME"
    python3 - <<EOF $MODEL_NAME
from ultralytics import YOLO
import openvino, sys, shutil
model = YOLO(sys.argv[1] + '.pt')
model.info()
converted_path = model.export(format='openvino')
converted_model = converted_path + '/' + sys.argv[1] +'.xml'
core = openvino.Core()
ov_model = core.read_model(model=converted_model)
ov_model.set_rt_info("YOLOv8-OBB", ['model_info', 'model_type'])
openvino.save_model(ov_model, './FP32/' + sys.argv[1] +'.xml', compress_to_fp16=False)
openvino.save_model(ov_model, './FP16/' + sys.argv[1] +'.xml', compress_to_fp16=True)
shutil.rmtree(converted_path)
EOF
  fi
fi

if [ "$MODEL" == "yolov9c" ] || [ "$MODEL" == "yolo_all" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="yolov9c"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODELS_PATH"/public/"$MODEL_NAME"
    cd "$MODELS_PATH"/public/"$MODEL_NAME"
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov9c.pt")
model.info()
model.export(format='openvino')  # creates 'yolov9c_openvino_model/'
EOF
    mv "${MODEL_NAME}"_openvino_model FP32/
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov9c.pt")
model.info()
model.export(format='openvino', half=True)  # creates 'yolov9c_openvino_model/'
EOF
    mv "${MODEL_NAME}"_openvino_model FP16/
    cd ../..
  fi
fi


if [[ "$MODEL" == "centerface" ]] || [[ "$MODEL" == "all" ]]; then
  MODEL_NAME="centerface"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP16/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$MODELS_PATH
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/Star-Clouds/CenterFace.git
    cd CenterFace/models/onnx
    ovc centerface.onnx --input "[1,3,768,1280]" 
    mv centerface.xml "$MODEL_PATH"
    mv centerface.bin "$MODEL_DIR"
    cd ../../..
    rm -rf CenterFace
    cd "$PREV_DIR"
    python3 - <<EOF $MODEL_PATH
import xml.etree.ElementTree as ET
from xml.dom import minidom
import sys

# Define the path to your XML file
xml_file_path = sys.argv[1]

# Load the XML file
tree = ET.parse(xml_file_path)
root = tree.getroot()

# Define the attributes to search for and the new names
layer_search_attributes = {'id': '438', 'name': '537/sink_port_0', 'type': 'Result', 'version': 'opset1'}
layer_new_name = 'heatmap/sink_port_0'

port_search_attributes = {'id': '1', 'precision': 'FP32', 'names': '537'}
port_new_name = 'heatmap'

# Update 'layer' elements
for layer in root.findall(".//layer[@id='{}'][@name='{}'][@type='{}'][@version='{}']".format(
        layer_search_attributes['id'],
        layer_search_attributes['name'],
        layer_search_attributes['type'],
        layer_search_attributes['version'])):
    layer.set('name', layer_new_name)

# Update 'port' elements
for port in root.findall(".//port[@id='{}'][@precision='{}'][@names='{}']".format(
        port_search_attributes['id'],
        port_search_attributes['precision'],
        port_search_attributes['names'])):
    port.set('names', port_new_name)

# Define the attributes to search for and the new names
layer_search_attributes = {'id': '430', 'name': '538/sink_port_0', 'type': 'Result', 'version': 'opset1'}
layer_new_name = 'scale/sink_port_0'

port_search_attributes = {'id': '2', 'precision': 'FP32', 'names': '538'}
port_new_name = 'scale'

# Update 'layer' elements
for layer in root.findall(".//layer[@id='{}'][@name='{}'][@type='{}'][@version='{}']".format(
        layer_search_attributes['id'],
        layer_search_attributes['name'],
        layer_search_attributes['type'],
        layer_search_attributes['version'])):
    layer.set('name', layer_new_name)

# Update 'port' elements
for port in root.findall(".//port[@id='{}'][@precision='{}'][@names='{}']".format(
        port_search_attributes['id'],
        port_search_attributes['precision'],
        port_search_attributes['names'])):
    port.set('names', port_new_name)

# Define the attributes to search for and the new names
layer_search_attributes = {'id': '423', 'name': '539/sink_port_0', 'type': 'Result', 'version': 'opset1'}
layer_new_name = 'offset/sink_port_0'

port_search_attributes = {'id': '2', 'precision': 'FP32', 'names': '539'}
port_new_name = 'offset'

# Update 'layer' elements
for layer in root.findall(".//layer[@id='{}'][@name='{}'][@type='{}'][@version='{}']".format(
        layer_search_attributes['id'],
        layer_search_attributes['name'],
        layer_search_attributes['type'],
        layer_search_attributes['version'])):
    layer.set('name', layer_new_name)

# Update 'port' elements
for port in root.findall(".//port[@id='{}'][@precision='{}'][@names='{}']".format(
        port_search_attributes['id'],
        port_search_attributes['precision'],
        port_search_attributes['names'])):
    port.set('names', port_new_name)

# Define the attributes to search for and the new names
layer_search_attributes = {'id': '416', 'name': '540/sink_port_0', 'type': 'Result', 'version': 'opset1'}
layer_new_name = 'landmarks/sink_port_0'

port_search_attributes = {'id': '2', 'precision': 'FP32', 'names': '540'}
port_new_name = 'landmarks'

# Update 'layer' elements
for layer in root.findall(".//layer[@id='{}'][@name='{}'][@type='{}'][@version='{}']".format(
        layer_search_attributes['id'],
        layer_search_attributes['name'],
        layer_search_attributes['type'],
        layer_search_attributes['version'])):
    layer.set('name', layer_new_name)

# Update 'port' elements
for port in root.findall(".//port[@id='{}'][@precision='{}'][@names='{}']".format(
        port_search_attributes['id'],
        port_search_attributes['precision'],
        port_search_attributes['names'])):
    port.set('names', port_new_name)


model_info = ET.Element('model_info')
confidence_threshold = ET.SubElement(model_info, 'confidence_threshold', {'value': '0.55'})
iou_threshold = ET.SubElement(model_info, 'iou_threshold', {'value': '0.5'})
model_type = ET.SubElement(model_info, 'model_type', {'value': 'centerface'})

rt_info_elements = root.findall('.//rt_info[Runtime_version]')
rt_info = rt_info_elements[0]
rt_info.append(model_info)

# Function to pretty print XML with proper indentation
def prettify(elem):
    """Return a pretty-printed XML string for the Element."""
    rough_string = ET.tostring(elem, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    pretty_xml = reparsed.toprettyxml(indent="\t")
    pretty_xml = '\n'.join([line for line in pretty_xml.split('\n') if line.strip()])
    return pretty_xml

# Get the pretty-printed XML string
pretty_xml = prettify(root)

#tree.write(xml_file_path, xml_declaration=True)
# Write the modified and pretty-printed XML back to the file
with open(xml_file_path, 'w', encoding='utf-8') as xml_file:
    xml_file.write(pretty_xml)
EOF

  fi
fi

if [ "$MODEL" == "hsemotion" ] || [ "$MODEL" == "all" ]; then
  MODEL_NAME="hsemotion"
  MODEL_PATH="$MODELS_PATH/public/$MODEL_NAME/FP16/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$MODELS_PATH
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/av-savchenko/face-emotion-recognition.git
    cd face-emotion-recognition/models/affectnet_emotions/onnx
    ovc enet_b0_8_va_mtl.onnx --input "[16,3,224,224]"
    mv enet_b0_8_va_mtl.xml "$MODEL_DIR"
    mv enet_b0_8_va_mtl.bin "$MODEL_DIR"
    cd ../../../..
    rm -rf face-emotion-recognition
    cd "$PREV_DIR"
    python3 - <<EOF $MODEL_PATH
EOF
  fi
fi