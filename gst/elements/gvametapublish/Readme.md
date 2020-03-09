**GVAMETAPUBLISH**

A GStreamer element to publish JSON data to a designated file, or a chosen message broker:

  1. File (default)

  2. MQTT broker

  3. Kafka broker

**Build components

1. Build libraries either through docker or on host machine

    Follow instructions here:
    https://github.com/opencv/gstreamer-plugins/wikis/Getting-Started-Guide-%5BR2%5D
    Docker image built with Dockerfile will include all necessary dependencies for kafka/mqtt.
    If building on host machine these dependencies will be resolved by the install_metapublish_dependencies.sh script

2. Run sample pipeline to test

    vehicle_detection_publish_file_batch.sh
    vehicle_detection_publish_file_stream.sh
    vehicle_detection_publish_mqtt.sh
    vehicle_detection_publish_kafka.sh

3. Create your own pipeline and add gvametapublish element with the following parameters:

    gvametapublish method=file filepath=/root/video-example/detections_2019.json
    1. Optionally provide outputformat=stream to have raw JSON inferences written to the file, or outputformat=batch (default) to have the file populated as an array of JSON inferences:

    gvametapublish method=file filepath="/root/video-examples/detections_2019.json" outputformat=stream ! \

    2. To publish data to mqtt broker:

    gvametapublish method=mqtt address=127.0.0.1:1883 clientid=clientIdValue topic=topicName timeout=timeoutValue

    3. To publish data to kafka broker:

    gvametapublish method=kafka address=127.0.0.1:9092 topic=topicName

Note: *method is a required property of gvametapublish element.
