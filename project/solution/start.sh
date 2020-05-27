#!/bin/bash

export HAND_DETECTION="gvaskeleton model_path=$MODELS_PATH/intel/human-pose-estimation-0001/FP32/human-pose-estimation-0001.xml device=CPU hands-detect=true ! gvaclassify model=$MODELS_PATH/hand_type/hand_type.xml model-proc=$MODELS_PATH/hand_type/hand_classify.json"

export SPEED_DETECTION="gvadetect model=$MODELS_PATH/intel/person-detection-retail-0013/FP32/person-detection-retail-0013.xml ! gvaspeedometer alpha=0.1 alpha-hw=0.01  interval=0.0333333333"

export FACE_DETECTION="gvadetect model=$MODELS_PATH/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml device=CPU pre-process-backend=opencv ! queue ! gvaclassify model=$MODELS_PATH/intel/landmarks-regression-retail-0009/FP32/landmarks-regression-retail-0009.xml model-proc=/root/gst-video-analytics/samples/gst_launch/reidentification/model_proc/landmarks-regression-retail-0009.json device=CPU pre-process-backend=opencv ! queue ! gvaclassify model=$MODELS_PATH/intel/face-reidentification-retail-0095/FP32/face-reidentification-retail-0095.xml model-proc=/root/gst-video-analytics/samples/gst_launch/reidentification/model_proc/face-reidentification-retail-0095.json device=CPU pre-process-backend=opencv ! queue ! gvaidentify gallery=/root/gst-video-analytics/samples/people_on_stairs/identify/gallery/gallery.json cosine-distance-threshold=0.0"

export FINALIZE="gvapython module=meta_collector.py class=MetaCollector"

python3 stairsguard_analytics.py