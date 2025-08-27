# ==============================================================================
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import os
import re
import yaml
import string
from argparse import ArgumentParser
import json
from jsonschema import validate

dldt_str = 'dl' + 'dt'
openvino_str = 'open' + 'vino'
dlstreamer_name = 'Deep ' + 'Learning' + ' Streamer'

parser = ArgumentParser(add_help=False)
_args = parser.add_argument_group('Options')
_args.add_argument("-mi", "--model_index", help="Path to all_models.yaml file", required=True, type=str)
_args.add_argument("-vm", "--verified_models", help="Path to supported_models.json file", required=True, type=str)
_args.add_argument("-o", "--output", help="Required. Path to output.rst file", required=True, type=str)
args = parser.parse_args()

models={}

with open(args.model_index) as f:
    models = yaml.safe_load(f)

with open(args.verified_models) as f:
    verified_models = json.load(f)

# write .rst
with open(args.output, 'w') as f:
    f.write('Supported Models\n')
    f.write('================\n')
    f.write('\n')
    f.write('This page contains the table of models supported by Deep Learning Streamer.')
    f.write('\n')
    f.write('Each model has a link (under the model name) to the original documentation with download instructions.\n')
    f.write('\n')
    f.write('Most models are from `OpenVINO™ Open Model Zoo <https://github.com/openvinotoolkit/open_model_zoo/>`__\n')
    f.write('but some of them come from other sources.\n')
    f.write('\n')
    '''
    f.write('Abbreviations used in the table\n')
    f.write('--------------------------------\n')
    f.write('.. list-table::\n')
    f.write('    :header-rows: 1\n')
    f.write('\n')
    f.write('    * - Abbreviation\n')
    f.write('      - Description\n')
    f.write('    * - {0}\n'.format(openvino_str))
    f.write('      - `OpenVINO™ toolkit <https://docs.openvino.ai/>`__ - as model file format ( .xml +  .bin) and inference backend in {0}\n'.format(dlstreamer_name))
    f.write('    * - pytorch\n')
    f.write('      - `PyTorch* framework <https://pytorch.org>`__ - as model file format and inference backend in {0}\n'.format(dlstreamer_name))
    f.write('    * - tf\n')
    f.write('      - `TensorFlow* framework <https://www.tensorflow.org>`__ - as model file format and inference backend in {0}\n'.format(dlstreamer_name))
    f.write('    * - onnx\n')
    f.write('      - `ONNX <https://onnx.ai>`__ - Open Neural Network Exchange file format\n')
    f.write('    * - caffe\n')
    f.write('      - `Caffe* framework <https://caffe2.ai/>`__ - as model file format\n')
    f.write('    * - dlstreamer\n')
    f.write('      - {0}\n'.format(dlstreamer_name))
    '''
    f.write('\n')
    f.write('Models Table\n')
    f.write('----------------\n')
    f.write('\n')
    f.write('.. list-table::\n')
    f.write('    :header-rows: 1\n')
    f.write('\n')
    f.write('    * - #\n')
    f.write('      - Category\n')
    f.write('      - Model Name\n')
    #f.write('      - Source Repo\n')
    #f.write('      - Available Format(s)\n')
    f.write('      - GFLOPs\n')
    #f.write('      - {0} support\n'.format(dlstreamer_name))
    #f.write('      - Open VINO™ support\n')
    #f.write('      - Py Torch* support\n')
    #f.write('      - Tensor Flow* support\n')
    f.write('      - labels-file\n')
    f.write('      - model-proc\n')
    f.write('      - Demo app\n')
    f.write('\n')
    n = 0
    for name in models:
        
        if name not in verified_models:
            continue

        m = models[name]


        name_s = name.replace('torchvision.models.detection.', 'torchvision.models.detection. ')
        n = n + 1
        f.write('    * - {0}\n'.format(n))
        f.write('      - {0}\n'.format(string.capwords(m.get('task_type', '').replace('_',' '))))
        f.write('      - `{0} <{1}>`__\n'.format(name_s, m.get('readme', '')))
        #f.write('      - {0}\n'.format(m.get('source', '')))
        #f.write('      - {0}\n'.format(m.get('format', '')))
        f.write('      - {0}\n'.format(re.split(r"[^0-9\.]", m.get('GFLOPs', m.get('GFlops', ' ')))[0]))
        #f.write('      - {0}\n'.format(m.get('dlstreamer_support', '')))
        #f.write('      - {0}\n'.format(m.get('openvino_devices', '?')))
        #f.write('      - {0}\n'.format(m.get('pytorch_devices', '')))
        #f.write('      - {0}\n'.format(m.get('tf_devices', '')))
        labels_file = m.get('labels-file', None)
        if labels_file:
            f.write('      - `{0} <{1}>`__\n'.format(os.path.basename(labels_file), labels_file))
        else:
            #f.write('      - {0}\n'.format(m.get('datasets', [{}])[0].get('name','').replace('-',' ').replace('_',' ')))
            f.write('      -\n')
        model_proc = m.get('model-proc', None)
        if model_proc:
            if model_proc[0:4] != 'http':
                f.write('      - {0}\n'.format(model_proc))
            else:
                f.write('      - `{0} <{1}>`__\n'.format("model-proc", model_proc))
        else:
            f.write('      -\n')
        #f.write('      - {0}\n'.format(m.get('omz_demo', '')))

        t1 = t2 =''
        if 'demo_apps' in m:
            t1 = m['demo_apps'][0]
        
        if 'demo_urls' in m:
            t2 = m['demo_urls'][0]

        if (t1 == '') or (t2 == ''):
            f.write('      -\n')
        else:
            f.write('      - `{0} <{1}>`__\n'.format(t1, t2))

    f.write('\n')
    f.write('Legal Information\n')
    f.write('-------------------\n')
    f.write('PyTorch, TensorFlow, Caffe, Keras, MXNet are trademarks or brand names of their respective owners.\n')
    f.write('All company, product and service names used in this website are for identification purposes only.\n')
    f.write('Use of these names,trademarks and brands does not imply endorsement.\n')
