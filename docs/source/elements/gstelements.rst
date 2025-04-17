GStreamer Elements
==================

This page constains examples of some GStreamer elements/plugins in combination with the Intel® Deep Learning Streamer (Intel® DL Streamer) elements.


.. list-table::
   :header-rows: 1

   * - Element
     - Description

   * - :doc:`compositor <compositor>`
     - The `compositor <https://gstreamer.freedesktop.org/documentation/compositor/?gi-language=c>`__ element allows merging multiple displays into one  

   * - timecodestamper
     - The `timecodestamper <https://gstreamer.freedesktop.org/documentation/timecode/timecodestamper.html?gi-language=c>`__ element allows attaching 
       a timecode into every incoming video frame.

       *[eg syntax]* *gst-launch-1.0 rtspsrc location="rtsp://root:admin_pwd@IP/axis-media/media.amp" ! rtph264depay ! h264parse ! avdec_h264 ! 
       timecodestamper set=always source=rtc ! videoconvert  ! gvadetect model=$mDetect device=CPU ! queue ! gvametaconvert timestamp-utc=true json-indent=-1 ! 
       gvametapublish method=mqtt file-format=json  mqtt-config=mqtt_config.json ! fakesink sync=false*

  
.. toctree::
   :maxdepth: 1
   :hidden:

   compositor

   
   
