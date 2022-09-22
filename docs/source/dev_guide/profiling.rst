Profiling with Intel VTune™
===========================

This page describes how to enable ITT tracing and analyze performance of Intel® Deep Learning Streamer (Intel® DL Streamer) and GStreamer elements using Intel VTune™ tool.

.. note::
   Intel VTune™ uses ITT interface to capture custom tasks and visualize them on Profile tab. ITT instrumentation enabled in default
   builds of Intel® DL Streamer, oneTBB, OpenCL intercept-layer, and some other libraries, but disabled by default in binary releases of
   OpenVINO™ toolkit. Please refer to `wiki page <https://github.com/openvinotoolkit/openvino/wiki/BuildingCode>`__
   for instructions how to build OpenVINO™ toolkit from sources and additionally pass ``-DENABLE_PROFILING_ITT=ON`` option to cmake in configuration step.
   More details about ITT instrumentation in OpenVINO™ toolkit can be found on
   `wiki page <https://github.com/openvinotoolkit/openvino/wiki/PerformanceAnalysisUsingITTcounters>`__

1. Install VTune™
-----------------

Download and install VTune™ using following link https://www.intel.com/content/www/us/en/developer/tools/oneapi/vtune-profiler-download.html

Note: If you want remote profiling you need to install VTune™ on both host and target system

2. Configure VTune™ target platform.
------------------------------------
1. Open VTune™ and create new project (or just Configure Analysis)
2. Specify your target platform Please refer following Guide: https://www.intel.com/content/www/us/en/develop/documentation/vtune-help/top/set-up-analysis-target.html
3. For remote connection please see: https://www.intel.com/content/www/us/en/develop/documentation/vtune-help/top/set-up-analysis-target/linux-targets/remote-linux-target-setup/configuring-ssh-access-for-remote-collection.html

3. Source OpenVINO™ toolkit and Intel® DL Streamer environment variables
-------------------------------
On target system source OpenVINO™ toolkit and DLStreamer environment variables as usual

.. code:: shell

    # OpenVINO™ toolkit environment
    source /opt/intel/openvino_2022/setupvars.sh
    
    # Intel® DL Streamer environment
    source /opt/intel/dlstreamer/setupvars.sh
    
    # Intel® oneAPI DPC++/C++ Compiler environment (if installed)
    # source /opt/intel/oneapi/compiler/latest/env/vars.sh

And additionally set GST_TRACERS environment variable to profile all GStreamer elements in pipeline

.. code:: shell

    # Enable itt tracing in DLStreamer pipeline
    export GST_TRACERS=gvaitttracer


4. VTune™ configuration
-----------------------

1. Choose Launch Application option.
2. Set Application path: full path do gst-launch-1.0 application like: 

    .. code:: shell

        /opt/intel/dlstreamer/gstreamer/bin/gst-launch-1.0

3. In application parameters just pass full Intel® DL Streamer pipeline starting from filesrc option. Like below:

    .. code:: shell
    
        filesrc location=<VIDEO_FILE> ! decodebin ! gvainference model=<MODEL>.xml ! fakesink sync=false

4. Update advanced options: ensure check-box ``Analyze child processes`` is set.

5. (Optional) If you launched VTune™ remotely or using different environment (CLI) you need to create a VTune™ wrapper script like below and set in under Advanced settings "Wrapper script" section:

    .. code:: shell

        #!/bin/shell
        command="$@"

        # OpenVINO™ Toolkit environment
        source /opt/intel/openvino_2022/setupvars.sh
    
        # Intel® DL Streamer environment
        source /opt/intel/dlstreamer/setupvars.sh

        # Run VTune™ collector
        $command

        # Postfix script
        ls -la $VTUNE_RESULT_DIR

6. On Analysis Type tab set check-box "Analyze user tasks, events and counters"

.. image:: https://docs.openvino.ai/2021.4/_images/vtune_option.png

7. Press start button to execute your pipeline and collect performance snapshot.

5. Results Analysis
-------------------
When results is ready you can check Bottom-UP tab (Grouping "Task Type / Function / Call stack") to check how much time each task takes and how many times it was called.

For example on screenshot below we can see whole pipeline duration was 75.2s (including models load step). Inference (gvadetect) in general took 9.068s of whole pipeline execution and was called 300 times (Because input media filed had 300 frames.). 
4.110s of this inference was taken by Inference completion callback (completion_callback_lambda) where DLStreamer processes inference results from OV. And about 0.445s for Submitting image. So it means the remaining time 9.068 - 4.110 - 0.445 = 4.513s was taken by executing inference inside OV.

.. figure:: BottomUP_tab.png
   :alt: Bottom-UP tab

Also you can check platform tab to see detailed calls graph and measure each methods call time.

For example on screenshot below shown how to measure first OpenVINO™ Toolkit inference time: the time from first Submit image till first completion callback call.

.. figure:: Platform_tab.png
   :alt: Platform tab
