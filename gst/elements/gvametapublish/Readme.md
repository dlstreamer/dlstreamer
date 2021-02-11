**GVAMETAPUBLISH**

A GStreamer element to publish JSON data to a designated file, or a chosen message broker:

  1. File (default)

  2. MQTT broker

  3. Kafka broker

## Build components

1. Build libraries either through docker or on host machine

    Follow instructions here:
    https://github.com/openvinotoolkit/dlstreamer_gst/wiki/Install-Guide#install-message-brokers-optional
    Docker image built with Dockerfile will include all necessary dependencies for kafka/mqtt.
    If building on host machine these dependencies will be resolved by the install_metapublish_dependencies.sh script

2. Run [metapublish](https://github.com/openvinotoolkit/dlstreamer_gst/blob/master/samples/gst_launch/metapublish/metapublish.sh) sample to test

    vehicle_detection_publish_file_json.sh
    vehicle_detection_publish_file_json_lines.sh
    vehicle_detection_publish_mqtt.sh
    vehicle_detection_publish_kafka.sh

3. Create your own pipeline and add gvametapublish element with the following parameters:

    ```bash
    gvametapublish method=file filepath=/root/video-example/detections_2019.json
    ```
    - Optionally provide file-format=json-lines to have raw JSON inferences written to the file, or file-format=json (default) to have the file populated as an array of JSON inferences:

        ```bash
        gvametapublish method=file filepath="/root/video-examples/detections_2019.json" file-format=json-lines
        ```

    2. To publish data to mqtt broker:

        ```bash
        gvametapublish method=mqtt address=127.0.0.1:1883 mqtt-client-id=clientIdValue topic=topicName timeout=timeoutValue
        ```

    - To publish data to kafka broker:

    gvametapublish method=kafka address=127.0.0.1:9092 topic=topicName

Note: \*method is a required property of gvametapublish element.
