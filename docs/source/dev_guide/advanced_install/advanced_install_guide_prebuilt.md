# Advanced Installation On Ubuntu - Using Pre-built Packages

> **NOTE:** Installation of Deep Learning Streamer Pipeline Framework
> [from pre-built Debian packages using one-click script](../../get_started/install/install_guide_ubuntu.md)
> is the easiest approach.

The instructions below focus on manual installation of pre-built
Debian packages.

## Step 1: Setup Prerequisites

Follow the instructions in
[the prerequisites](../../get_started/install/install_guide_ubuntu#prerequisites) section.

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

Download pre-built Debian packages:

::::{tab-set}
:::{tab-item} Ubuntu 24.04
:sync: tab1
  ```bash
  mkdir -p ~/intel/dlstreamer_gst
  cd ~/intel/dlstreamer_gst
  wget $(wget -q -O - https://api.github.com/repos/dlstreamer/dlstreamer/releases/latest | \
    jq -r '.assets[] | select(.name | contains ("ubuntu_24.04_amd64.deb")) | .browser_download_url')
  ```
:::
:::{tab-item} Ubuntu 22.04
:sync: tab2
  ```bash
  cd ~/intel/dlstreamer_gst
  wget $(wget -q -O - https://api.github.com/repos/dlstreamer/dlstreamer/releases/latest | \
    jq -r '.assets[] | select(.name | contains ("ubuntu_22.04_amd64.deb")) | .browser_download_url')
  ```
:::
::::

## Step 3: Install Deep Learning Streamer

Install Deep Learning Streamer from pre-built Debian packages:

```bash
sudo apt install ./*.deb
```

## Step 4: Install OpenVINO™ toolkit

Install Intel® OpenVINO™, using the `install_openvino.sh` script.

```bash
sudo -E /opt/intel/dlstreamer/install_dependencies/install_openvino.sh
```

## Step 5: (Optional) Install MQTT and Kafka clients for element `gvametapublish`

To enable all `gvametapublish` backends install required dependencies:

```bash
sudo -E /opt/intel/dlstreamer/install_dependencies/install_mqtt_client.sh
sudo -E /opt/intel/dlstreamer/install_dependencies/install_kafka_client.sh
```

## Step 6: Add user to groups

When using Media, GPU or NPU devices as non-root user, add your
user to `video` and `render` groups:

```bash
sudo usermod -a -G video <username>
sudo usermod -a -G render <username>
```

## Step 7: Set up the environment for Deep Learning Streamer

Source the required environment variables to run GStreamer and Deep Learning
Streamer:

```bash
# Setup OpenVINO™ Toolkit environment
source /opt/intel/openvino_2024/setupvars.sh
# Setup GStreamer and Deep Learning Streamer Pipeline Framework environments
source /opt/intel/dlstreamer/setupvars.sh
```

> **NOTE:**
> The environment variables are removed when you close the shell. Before
> each run of Deep Learning Streamer you need to setup the environment with
> the two scripts listed in this step. Optionally, to automate the process, you
> can add the variables to the `~/.bashrc` file for every shell session.

## Step 8: Verify Deep Learning Streamer installation

Deep Learning Streamer has been installed. You can run the
`gst-inspect-1.0 gvadetect` to confirm that GStreamer and Deep Learning
Streamer are running:

```bash
gst-inspect-1.0 gvadetect
```

When the installation completes, help information for `gvadetect` element
is displayed:

![image](../../get_started/install/gvadetect_sample_help.png)

## Step 9: Next steps - running sample Deep Learning Streamer pipelines

You are ready to use Deep Learning Streamer. For further instructions on how to run
sample pipeline(s), see
[the installation guide](../../get_started/install/install_guide_ubuntu).

------------------------------------------------------------------------

> **\*** *Other names and brands may be claimed as the property of
> others.*
