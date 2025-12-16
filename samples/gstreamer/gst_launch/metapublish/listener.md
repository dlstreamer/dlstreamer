# MetaPublish Listeners
Before you run the metapublish.sh sample with output to MQTT or Kafka or FIFO, add a listener that corresponds to the method you attempt.

By default the sample will display pretty printed output to stdout. But you may provide parameters to output to MQTT or Kafka or FIFO, in this case
* For MQTT, install and launch MQTT broker along with an MQTT client listener subscribed to the topic (that emits to console).
* For Kafka, install and launch Zookeeper and Kafka broker along with a Kafka client listener subscribed to the topic (that emits to console).
* For FIFO file, create the fifo and tail results in console.

## MQTT
Mosquitto provides a Docker image to quickly stand up a minimal MQTT broker and client listener.

Create a listen_mqtt.sh script that looks similar to this:
```
#!/bin/bash
#listen_mqtt.sh

# Launch minimal MQTT broker
docker run -d --rm --name dlstreamer_mqtt -p 1883:1883 -p 9001:9001 eclipse-mosquitto:1.6
echo "Listening for MQTT messages on 'dlstreamer' topic..."
# Launch client listener, subscribing to default topic
# Emit output to console
mosquitto_sub -h localhost -t dlstreamer
```

## Kafka
Bitnami provides Docker images to quickly stand up a minimal Kafka broker and client listener.

1. Construct a Kafka compose file named docker-compose-kafka.yml.
```
# Get Bitnami compose file
echo "Fetching docker-compose-kafka.yml"
curl -sSL https://raw.githubusercontent.com/bitnami/bitnami-docker-kafka/master/docker-compose.yml > docker-compose-kafka.yml

# Modify docker-compose-kafka.yml to allow auto-creation of topics
# Modify docker-compose-kafka.yml to add distinct network
```

The new compose file should match the following:
```
#docker-compose-kafka.yml
version: '2'

networks:
  kafka-net:
    driver: bridge

services:
  zookeeper:
    image: 'bitnami/zookeeper:latest'
    networks:
      - kafka-net
    ports:
      - '2181:2181'
    environment:
      - ALLOW_ANONYMOUS_LOGIN=yes
  kafka:
    image: 'bitnami/kafka:latest'
    networks:
      - kafka-net
    ports:
      - '9092:9092'
    environment:
      - KAFKA_CFG_AUTO_CREATE_TOPICS_ENABLE=true
      - KAFKA_CFG_ZOOKEEPER_CONNECT=zookeeper:2181
      - KAFKA_CFG_ADVERTISED_LISTENERS=PLAINTEXT://localhost:9092
      - ALLOW_PLAINTEXT_LISTENER=yes
    depends_on:
      - zookeeper
```

2. Create a listen_kafka.sh script in the same folder as your compose file that looks similar to this:
```
#!/bin/bash
#listen_kafka.sh

echo "Launching zookeeper and kafka containers with auto-topic creation on a distinct network."
docker-compose -p metapublish -f docker-compose-kafka.yml up -d

echo "Listening for Kafka messages on 'dlstreamer' topic..."
# Emit output to console
docker exec -it metapublish_kafka_1 /opt/bitnami/kafka/bin/kafka-console-consumer.sh --bootstrap-server localhost:9092 --topic dlstreamer
```

## FIFO file
Create the FIFO file and tail results in console

```
#!/bin/bash
#listen_fifo.sh
OUTPUT_PATHFILE=./dlstreamer.fifo

# Assure previous fifo does not exist and create one using same name provided to metapublish.sh
rm -f $OUTPUT_PATHFILE
mkfifo $OUTPUT_PATHFILE

echo "Waiting for Deep Learning Streamer (DL Streamer) to write lines to FIFO: $OUTPUT_PATHFILE"

# Emit output to console
tail -f $OUTPUT_PATHFILE
```
