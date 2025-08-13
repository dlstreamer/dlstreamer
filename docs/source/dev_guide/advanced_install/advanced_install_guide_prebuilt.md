# Ubuntu advanced installation - pre-built packages

The easiest way to install Intel® Deep Learning Streamer (Intel® DL
Streamer) Pipeline Framework is installing it from pre-built Debian
packages with one-click script. If you would like to follow this way,
please see [the installation guide](../../get_started/install/install_guide_ubuntu).

The instruction below focuses on installation steps with pre-built
Debian packages performed manually.

## Step 1: Install prerequisites

Please go through prerequisites 1 & 2 described in
[the installation guide](../../get_started/install/install_guide_ubuntu).

## Step 2: Prepare the installation environment

Download Ninja build system and use it to build OpenCV library:

```bash
mkdir -p ~/intel/dlstreamer_gst
cd ~/intel/dlstreamer_gst
sudo apt-get install ninja-build unzip
wget -q --no-check-certificate -O opencv.zip https://github.com/opencv/opencv/archive/4.10.0.zip
unzip opencv.zip && rm opencv.zip && mv opencv-4.10.0 opencv && mkdir -p opencv/build
cd ./opencv/build
cmake -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -GNinja .. \
&& ninja -j "$(nproc)" && sudo ninja install
```

A. For **Ubuntu 24.04**:

    Download pre-built Debian packages:

    ```bash
    mkdir -p ~/intel/dlstreamer_gst
    cd ~/intel/dlstreamer_gst
    wget $(wget -q -O - https://api.github.com/repos/dlstreamer/dlstreamer/releases/latest | \
      jq -r '.assets[] | select(.name | contains ("ubuntu_24.04_amd64.deb")) | .browser_download_url')
    ```

B. For **Ubuntu 22.04**:

    Download pre-built Debian packages:

    ```bash
    cd ~/intel/dlstreamer_gst
    wget $(wget -q -O - https://api.github.com/repos/dlstreamer/dlstreamer/releases/latest | \
      jq -r '.assets[] | select(.name | contains ("ubuntu_22.04_amd64.deb")) | .browser_download_url')
    ```

## Step 3: Install Intel® DL Streamer

Install Intel® DL Streamer from pre-built Debian packages:

```bash
sudo apt install ./*.deb
```

## Step 4: Install OpenVINO™ toolkit

Install Intel® OpenVINO™ via script `install_openvino.sh`.

```bash
sudo -E /opt/intel/dlstreamer/install_dependencies/install_openvino.sh
```

## Step 5: Install MQTT and Kafka clients for element `gvametapublish`

> **NOTE:**  Optional step: In order to enable all `gvametapublish` backends install
> required dependencies with scripts:

```bash
sudo -E /opt/intel/dlstreamer/install_dependencies/install_mqtt_client.sh
sudo -E /opt/intel/dlstreamer/install_dependencies/install_kafka_client.sh
```

## Step 6: Add user to groups

When using Media, GPU or NPU devices as non-root user, please add your
user to `video` and `render` groups:

```bash
sudo usermod -a -G video <username>
sudo usermod -a -G render <username>
```

## Step 7: Set up the environment for Intel® DL Streamer

Source required environment variables to run GStreamer and Intel® DL
Streamer:

```bash
# Setup OpenVINO™ Toolkit environment
source /opt/intel/openvino_2024/setupvars.sh
# Setup GStreamer and Intel® DL Streamer Pipeline Framework environments
source /opt/intel/dlstreamer/setupvars.sh
```

> **NOTE:**
> The environment variables are removed when you close the shell. Before
> each run of Intel® DL Streamer you need to setup the environment with
> the 2 scripts listed in this step. As an option, you can set the
> environment variables in file `~/.bashrc` for automatic enabling.

## Step 8: Verify Intel® DL Streamer installation

Intel® DL Streamer has been installed. You can run the
`gst-inspect-1.0 gvadetect` to confirm that GStreamer and Intel® DL
Streamer are running:

```bash
gst-inspect-1.0 gvadetect
```

If your can see the documentation of `gvadetect` element, the
installation process is completed.

![image](../../get_started/install/gvadetect_sample_help.png)

## Step 9: Next steps - running sample Intel® DL Streamer pipelines

You are ready to use Intel® DL Streamer. For further instructions to run
sample pipeline(s), please see
[the installation guide](../../get_started/install/install_guide_ubuntu).

------------------------------------------------------------------------

> **\*** *Other names and brands may be claimed as the property of
> others.*
