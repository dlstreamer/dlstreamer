# Latency Tracer

In this tutorial, you will learn how to use `latency_tracer` to find
element and pipeline frame latency. The time displayed in the logs has
precision in the order of milliseconds.

- element latency - time that a
  buffer takes to travel from the source pad of the previous element to the source
  pad of the current element.
- pipeline latency - time that a buffer takes
  to travel from the source pad of the source element to the source pad of the
  element before the sink element. This also provides latency and fps of full
  pipeline.

## Pipeline

```bash
GST_DEBUG="GST_TRACER:7" GST_TRACERS="latency_tracer" gst-launch-1.0 videotestsrc num-buffers=500 ! videoconvert  ! avenc_h263p ! fakesink
```

## Sample Output

```bash
0:00:03.014774818 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element_interval, name=(string)avenc_h263p0, interval=(double)1000.015783, avg=(double)0.259879, min=(double)0.206307, max=(double)0.587317;
0:00:03.014782371 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_pipeline, frame_latency=(double)0.240226, avg=(double)0.255766, min=(double)0.205778, max=(double)0.588363, latency=(double)0.340871, fps=(double)2933.657880, frame_num=(uint)8801;
0:00:03.014861287 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element, name=(string)videoconvert0, frame_latency=(double)0.000578, avg=(double)0.000571, min=(double)0.000423, max=(double)0.009837, frame_num=(uint)8802, is_bin=(boolean)0;
0:00:03.014865552 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element_interval, name=(string)videoconvert0, interval=(double)1000.019802, avg=(double)0.000578, min=(double)0.000423, max=(double)0.006638;
0:00:03.015110029 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element, name=(string)avenc_h263p0, frame_latency=(double)0.248802, avg=(double)0.255194, min=(double)0.205306, max=(double)0.587317, frame_num=(uint)8802, is_bin=(boolean)0;
0:00:03.015115274 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_pipeline, frame_latency=(double)0.249380, avg=(double)0.255765, min=(double)0.205778, max=(double)0.588363, latency=(double)0.340871, fps=(double)2933.658362, frame_num=(uint)8802;
0:00:03.015119796 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_pipeline_interval, interval=(double)1000.020829, avg=(double)0.260457, min=(double)0.206755, max=(double)0.588363, latency=(double)0.347350, fps=(double)2878.940035;
```

By default, `latency_tracer` calculates latency for both pipeline and
elements. You have an option to select only pipeline or elements latency
using flags.

### Settings Flags

## Pipeline latency flag

```bash
GST_DEBUG="GST_TRACER:7" GST_TRACERS="latency_tracer(flags=pipeline)" gst-launch-1.0 videotestsrc num-buffers=500 ! videoconvert  ! avenc_h263p ! fakesink
```

Sample Output:

```bash
0:00:03.015115274 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_pipeline, frame_latency=(double)0.249380, avg=(double)0.255765, min=(double)0.205778, max=(double)0.588363, latency=(double)0.340871, fps=(double)2933.658362, frame_num=(uint)8802;
0:00:03.015119796 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_pipeline_interval, interval=(double)1000.020829, avg=(double)0.260457, min=(double)0.206755, max=(double)0.588363, latency=(double)0.347350, fps=(double)2878.940035;
```

## Element latency flag

```bash
GST_DEBUG="GST_TRACER:7" GST_TRACERS="latency_tracer(flags=element)" gst-launch-1.0 videotestsrc num-buffers=500 ! videoconvert  ! avenc_h263p ! fakesink
```

Sample Output:

```bash
0:00:03.014774818 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element_interval, name=(string)avenc_h263p0, interval=(double)1000.015783, avg=(double)0.259879, min=(double)0.206307, max=(double)0.587317;
0:00:03.014861287 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element, name=(string)videoconvert0, frame_latency=(double)0.000578, avg=(double)0.000571, min=(double)0.000423, max=(double)0.009837, frame_num=(uint)8802, is_bin=(boolean)0;
0:00:03.014865552 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element_interval, name=(string)videoconvert0, interval=(double)1000.019802, avg=(double)0.000578, min=(double)0.000423, max=(double)0.006638;
0:00:03.015110029 79459 0x55736c57a060 TRACE             GST_TRACER :0:: latency_tracer_element, name=(string)avenc_h263p0, frame_latency=(double)0.248802, avg=(double)0.255194, min=(double)0.205306, max=(double)0.587317, frame_num=(uint)8802, is_bin=(boolean)0;
```

### Settings interval

The interval value can be configured. The value must be set in milliseconds,
the default equals `1000`.

## Setting interval example

```bash
GST_DEBUG="GST_TRACER:7" GST_TRACERS="latency_tracer(flags=pipeline,interval=100)" gst-launch-1.0 videotestsrc num-buffers=500 ! videoconvert  ! avenc_h263p ! fakesink
```

Sample Output:

```bash
0:00:00.116094526 79468 0x556002934460 TRACE             GST_TRACER :0:: latency_tracer_pipeline_interval, interval=(double)100.219905, avg=(double)0.256772, min=(double)0.236046, max=(double)0.386057, latency=(double)0.345586, fps=(double)2893.636748;
0:00:00.116443843 79468 0x556002934460 TRACE             GST_TRACER :0:: latency_tracer_pipeline, frame_latency=(double)0.248532, avg=(double)0.256743, min=(double)0.236046, max=(double)0.386057, latency=(double)0.345622, fps=(double)2893.338508, frame_num=(uint)291;
```
