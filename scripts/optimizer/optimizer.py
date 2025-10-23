# ==============================================================================
# Copyright (C) 2025-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import argparse
import time
import logging
import itertools
import os
import subprocess

import gi
gi.require_version("Gst", "1.0")
from gi.repository import Gst

####################################### Init ######################################################

Gst.init()
logging.basicConfig(level=logging.INFO, format="[%(name)s] [%(levelname)8s] - %(message)s")
logger = logging.getLogger(__name__)
logger.info("GStreamer initialized successfully")
gst_version = Gst.version()
logger.info("GStreamer version: %d.%d.%d",
            gst_version.major,
            gst_version.minor,
            gst_version.micro)

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

def log_parameters_of_interest(pipeline):
    for element in pipeline:
        if "gvadetect" in element:
            parameters = parse_element_parameters(element)
            logger.info("Found Gvadetect, device: %s, batch size: %s, nireqs: %s",
                        parameters.get("device", "not set"),
                        parameters.get("batch-size", "not set"),
                        parameters.get("nireq", "not set"))

        if "gvaclassify" in element:
            parameters = parse_element_parameters(element)
            logger.info("Found Gvaclassify, device: %s, batch size: %s, nireqs: %s",
                        parameters.get("device", "not set"),
                        parameters.get("batch-size", "not set"),
                        parameters.get("nireq", "not set"))

###################################### System Scanning ############################################

def scan_system():
    context = {"GPU": False,
               "NPU": False}

    # check for presence of GPU
    try:
        gpu_query = subprocess.run(["dpkg", "-l", "intel-opencl-icd"],
                                   stderr=subprocess.DEVNULL,
                                   stdout=subprocess.DEVNULL,
                                   check=False)
        gpu_dir = os.listdir("/dev/dri")
        for file in gpu_dir:
            if "render" in file and gpu_query.returncode == 0:
                context["GPU"] = True

    # can happen on missing directory, signifies no GPU support
    except Exception: # pylint: disable=broad-exception-caught
        pass

    if context["GPU"]:
        logger.info("Detected GPU Device")
    else:
        logger.info("No GPU Device detected")

    # check for presence of NPU
    try:
        npu_query = subprocess.run(["dpkg", "-l", "intel-driver-compiler-npu"],
                                   stderr=subprocess.DEVNULL,
                                   stdout=subprocess.DEVNULL,
                                   check=False)
        npu_dir = os.listdir("/dev/accel/")
        for file in npu_dir:
            if "accel" in file and npu_query.returncode == 0:
                context["NPU"] = True

    # can happen on missing directory, signifies no NPU support
    except Exception: # pylint: disable=broad-exception-caught
        pass

    if context["NPU"]:
        logger.info("Detected NPU Device")
    else:
        logger.info("No NPU Device detected")

    return context

##################################### Pipeline Running ############################################

def explore_pipelines(suggestions, base_fps, search_duration, sample_duration):
    best_pipeline = []
    start_time = time.time()
    best_fps = base_fps
    for combination in itertools.product(*suggestions):
        combination = list(combination)
        log_parameters_of_interest(combination)

        try:
            fps = sample_pipeline(combination, sample_duration)

            if fps > best_fps:
                best_fps = fps
                best_pipeline = combination

        except Exception as e:
            logger.debug("Pipeline failed to start: %s", e)

        cur_time = time.time()
        if cur_time - start_time > search_duration:
            break

    return best_pipeline, best_fps

def sample_pipeline(pipeline, sample_duration):
    pipeline = pipeline.copy()

    # check if there is an fps counter after the last inference element
    for i, element in enumerate(reversed(pipeline)):
        # exit early if one is found before other elements
        if "gvafpscounter" in element:
            break

        # add one if no counter was found before inference elements
        if "gvadetect" in element or "gvaclassify" in element:
            pipeline.insert(len(pipeline) - i, "gvafpscounter")

    pipeline = "!".join(pipeline)
    logger.debug("Testing: %s", pipeline)

    pipeline = Gst.parse_launch(pipeline)

    logger.info("Sampling for %s seconds...", str(sample_duration))
    fps_counter = next(filter(lambda element: "gvafpscounter" in element.name, reversed(pipeline.children))) # pylint: disable=line-too-long

    bus = pipeline.get_bus()

    pipeline.set_state(Gst.State.PLAYING)
    terminate = False
    start_time = time.time()
    while not terminate:
        time.sleep(1)

        # Incorrect pipelines sometimes get stuck in Ready state instead of failing.
        # Terminate in those cases.
        _, state, _ = pipeline.get_state(Gst.CLOCK_TIME_NONE)
        if state == Gst.State.READY:
            del pipeline
            raise RuntimeError("Pipeline not healthy, terminating early")

        cur_time = time.time()
        if cur_time - start_time > sample_duration:
            terminate = True

    pipeline.set_state(Gst.State.NULL)

    # Process any messages from the bus
    message = bus.pop()
    while message is not None:
        if message.type == Gst.MessageType.ERROR:
            error, _ = message.parse_error()
            logger.error("Pipeline error: %s", error.message)
        elif message.type == Gst.MessageType.WARNING:
            warning, _ = message.parse_warning()
            logger.warning("Pipeline warning: %s", warning.message)
        elif message.type == Gst.MessageType.STATE_CHANGED:
            old, new, _ = message.parse_state_changed()
            logger.debug("State changed: %s -> %s ", old, new)
        else:
            logger.error("Other message: %s", str(message))
        message = bus.pop()

    del pipeline
    fps = fps_counter.get_property("avg-fps")
    logger.debug("Sampled fps: %f.2", fps)
    return fps

######################################## Preprocess ###############################################

def preprocess_pipeline(pipeline):
    for i, element in enumerate(pipeline):
        if "decodebin" in element:
            pipeline[i] = "decodebin3"

        if "vaapipostproc" in element:
            pipeline[i] = "vapostproc"

        if "vaapi-surface-sharing" in element:
            pipeline[i] = "va-surface-sharing"

#################################### Gvadetect & Gvaclassify ######################################

def add_gvadetect_suggestions(suggestions, context):
    add_classification_suggestions("gvadetect", suggestions, context)

def add_gvaclassify_suggestions(suggestions, context):
    add_classification_suggestions("gvaclassify", suggestions, context)

def add_classification_suggestions(element, suggestions, context):
    if context["GPU"]:
        add_parameter_suggestions(element, "GPU", "va-surface-sharing", suggestions)

    if context["NPU"]:
        add_parameter_suggestions(element, "NPU", "va", suggestions)

    add_parameter_suggestions(element, "CPU", "opencv", suggestions)


def add_parameter_suggestions(element, device, backend, suggestions):
    batches = [1, 2, 4, 8, 16, 32]
    nireqs = range(1, 9)
    for suggestion in suggestions:
        if element in suggestion[0]:
            parameters = parse_element_parameters(suggestion[0])

            for batch in batches:
                for nireq in nireqs:
                    parameters["device"] = device
                    parameters["pre-process-backend"] = backend
                    parameters["batch-size"] = str(batch)
                    parameters["nireq"] = str(nireq)
                    suggestion.append(f"{element} {assemble_parameters(parameters)}")

####################################### Main Logic ################################################

# Steps of pipeline optimization:
# 1. Measure the baseline pipeline's performace.
# 2. Pre-process the pipeline to cover cases where we're certain of the best alternative.
# 3. Run the pipeline through generators that provide suggestions for element alternatives.
# 4. Create a cartesian product of the suggestions
#    and start running the combinations to measure performance.
# 5. Any time a better pipeline is found, save it and its performance information.
# 6. Return the best discovered pipeline.
def get_optimized_pipeline(pipeline, search_duration = 300, sample_duration = 10):
    context = scan_system()

    pipeline = " ".join(pipeline).split("!")

    # Measure the performance of the original pipeline
    try:
        fps = sample_pipeline(pipeline, sample_duration)
    except Exception as e:
        logger.error("Pipeline failed to start, unable to measure fps: %s", e)
        raise RuntimeError("Provided pipeline is not valid") from e

    logger.info("FPS: %f.2", fps)

    # Replace any elements that we're sure have a best-in-class alternatives.
    preprocess_pipeline(pipeline)

    # Prepare the suggestions structure
    # Suggestions structure:
    #   [
    #       ["element1 param1=value1", "element1 param1=value2", ...other variants],
    #       ["element2 param1=value1", "element2 param1=value2", ...other variants],
    #       ["element3 param1=value1", "element3 param1=value2", ...other variants],
    #       ...other pipeline elements
    #   ]
    suggestions = []
    for element in pipeline:
        suggestions.append([element])

    # Collect suggestions for pipeline improvements
    add_gvadetect_suggestions(suggestions, context)
    add_gvaclassify_suggestions(suggestions, context)

    # Explore the suggestions and try to discover pipelines with better performance
    best_pipeline, best_fps = explore_pipelines(suggestions, fps, search_duration, sample_duration)

    # Fall back in case no better pipeline was found.
    if not best_pipeline:
        best_pipeline = pipeline
        best_fps = fps

    # Reconstruct the pipeline as a single string and return it.
    return "!".join(best_pipeline), best_fps

def main():
    parser = argparse.ArgumentParser(
        prog="DLStreamer Pipeline Optimization Tool",
        description="Use this tool to try and find versions of your pipeline that will run with increased performance." # pylint: disable=line-too-long
    )
    parser.add_argument("--search-duration", default=300,
                        help="Duration of time which should be spent searching for optimized pipelines (default: %(default)ss)") # pylint: disable=line-too-long
    parser.add_argument("--sample-duration", default=10,
                        help="Duration of sampling individual pipelines. Longer duration should offer more stable results (default: %(default)ss)") # pylint: disable=line-too-long
    parser.add_argument("pipeline", nargs="+",
                        help="Pipeline to be analyzed")
    args=parser.parse_args()

    try:
        best_pipeline, best_fps = get_optimized_pipeline(args.pipeline,
                                                         args.search_duration,
                                                         args.sample_duration)
        logger.info("\nBest found pipeline: %s \nwith fps: %f.2", best_pipeline, best_fps)
    except Exception as e: # pylint: disable=broad-exception-caught
        logger.error("Failed to optimize pipeline: %s", e)

if __name__ == "__main__":
    main()
