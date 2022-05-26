gvametapublish
==============

Publishes the JSON metadata to MQTT or Kafka message brokers or files.
MQTT and Kafka methods offer reconnection in case a connection to the
broker is not established or is lost. During this reconnection time, any
metadata that passes through the element will not be cached or
published. It will simply pass through to the next element in the
pipeline.

.. code-block:: none

  Pad Templates:
    SRC template: 'src'
      Availability: Always
      Capabilities:
        ANY

    SINK template: 'sink'
      Availability: Always
      Capabilities:
        ANY

  Element has no clocking capabilities.
  Element has no URI handling capabilities.

  Pads:
    SINK: 'sink'
      Pad Template: 'sink'
    SRC: 'src'
      Pad Template: 'src'

  Element Properties:
    address             : [method= kafka | mqtt] Broker address
                          flags: readable, writable
                          String. Default: null
    file-format         : [method= file] Structure of JSON objects in the file
                          flags: readable, writable
                          Enum "GstGVAMetaPublishFileFormat" Default: 1, "json"
                            (1): json             - the whole file is valid JSON array where each element is inference results per frame
                            (2): json-lines       - each line is valid JSON with inference results per frame
    file-path           : [method= file] Absolute path to output file for publishing inferences.
                          flags: readable, writable
                          String. Default: "stdout"
    max-connect-attempts: [method= kafka | mqtt] Maximum number of failed connection attempts before it is considered fatal.
                          flags: readable, writable
                          Unsigned Integer. Range: 1 - 10 Default: 1
    max-reconnect-interval: [method= kafka | mqtt] Maximum time in seconds between reconnection attempts. Initial interval is 1 second and will be doubled on each failure up to this maximum interval.
                          flags: readable, writable
                          Unsigned Integer. Range: 1 - 300 Default: 30
    method              : Publishing method. Set to one of: 'file', 'mqtt', 'kafka'
                          flags: readable, writable
                          Enum "GstGVAMetaPublishMethod" Default: 1, "file"
                            (1): file             - File publish
                            (2): mqtt             - MQTT publish
                            (3): kafka            - Kafka publish
    mqtt-client-id      : [method= mqtt] Unique identifier for the MQTT client. If not provided, one will be generated for you.
                          flags: readable, writable
                          String. Default: null
    name                : The name of the object
                          flags: readable, writable
                          String. Default: "gvametapublish0"
    parent              : The parent of the object
                          flags: readable, writable
                          Object of type "GstObject"
    qos                 : Handle Quality-of-Service events
                          flags: readable, writable
                          Boolean. Default: false
    signal-handoffs     : Send signal before pushing the buffer
                          flags: readable, writable
                          Boolean. Default: false
    topic               : [method= kafka | mqtt] Topic on which to send broker messages
                          flags: readable, writable
                          String. Default: null

  Element Signals:
    "handoff" :  void user_function (GstElement* object,
                                    GstBuffer* arg0,
                                    gpointer user_data);
