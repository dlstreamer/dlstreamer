# GStreamer Elements

This page contains examples of some GStreamer elements/plugins in
combination with the Intel® Deep Learning Streamer (Intel® DL Streamer)
elements.

| Element | Description |
|---|---|
| compositor | The compositor element allows merging multiple displays into one |
| timecodestamper | The timecodestamper element allows attaching<br>a timecode into every incoming video frame.<br>[eg syntax] gst-launch-1.0 rtspsrc location=”rtsp://root:admin_pwd@IP/axis-media/media.amp” ! rtph264depay ! h264parse ! avdec_h264 !<br>timecodestamper set=always source=rtc ! videoconvert  ! gvadetect model=$mDetect device=CPU ! queue ! gvametaconvert timestamp-utc=true json-indent=-1 !<br>gvametapublish method=mqtt file-format=json  mqtt-config=mqtt_config.json ! fakesink sync=false <br> |

:::{.toctree}
:maxdepth: 1
:hidden:

compositor
:::
