# ==============================================================================
# Copyright (C) 2025-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

from openvino import Core

def add_device_suggestions(suggestions):
    element_device_suggestions(suggestions, "gvadetect")
    element_device_suggestions(suggestions, "gvaclassify")

def element_device_suggestions(suggestions, element):
    devices = Core().available_devices
    for suggestion in suggestions:
        if element in suggestion[0]:
            for device in devices:
                parameters = parse_element_parameters(suggestion[0])
                if device in parameters.get("device", ""):
                    continue

                if "GPU" in device:
                    parameters["pre-process-backend"] = "va-surface-sharing"
                    memory = "video/x-raw(memory:VAMemory)"

                if "NPU" in device:
                    parameters["pre-process-backend"] = "va"
                    memory = "video/x-raw(memory:VAMemory)"

                if "CPU" in device:
                    parameters["pre-process-backend"] = "opencv"
                    memory = "video/x-raw"

                parameters["device"] = device
                suggestion.append(f" vapostproc ! {memory} ! {element} {assemble_parameters(parameters)}")

def add_batch_suggestions(suggestions):
    batches = [1, 2, 4, 8, 16, 32]
    for suggestion in suggestions:
        for element in ["gvadetect", "gvaclassify"]:
            if element in suggestion[0]:
                parameters = parse_element_parameters(suggestion[0])
                for batch in batches:
                    parameters["batch-size"] = str(batch)
                    suggestion.append(f" {element} {assemble_parameters(parameters)}")


def add_nireq_suggestions(suggestions):
    nireqs = range(1, 9)
    for suggestion in suggestions:
        for element in ["gvadetect", "gvaclassify"]:
            if element in suggestion[0]:
                parameters = parse_element_parameters(suggestion[0])
                for nireq in nireqs:
                    parameters["nireq"] = str(nireq)
                    suggestion.append(f" {element} {assemble_parameters(parameters)}")

####################################### Utils #####################################################

def parse_element_parameters(element):
    parameters = element.strip().split(" ")
    del parameters[0]
    parsed_parameters = {}
    for parameter in parameters:
        parts = parameter.split("=")
        parsed_parameters[parts[0]] = parts[1]

    return parsed_parameters

def assemble_parameters(parameters):
    result = ""
    for parameter, value in parameters.items():
        result = result + parameter + "=" + value + " "

    return result
