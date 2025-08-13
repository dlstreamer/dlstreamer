# Custom GStreamer Plugin Installation

This page provides steps how to install custom GStreamer plugin.

## 1. Install custom GStreamer plugin(s)

First, install dedicated GStreamer plugin(s) relevant for required,
missed GStreamer element(s) using apt package manager.

Example for installing GStreamer plugin `x264enc`

```bash
sudo apt update
sudo apt install gstreamer1.0-plugins-ugly
```

More generic case

```bash
sudo apt update
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio
```

## 2. Update GStreamer plugin(s) settings

After installation, update the *GST_PLUGIN_PATH* environment variable
about the path to GStreamer custom plugin(s) directory installed in the
[step 1](#install-custom-gstreamer-plugins)

Example of updating `GST_PLUGIN_PATH` about path to the custom plugin
i.e. `/usr/lib/x86_64-linux-gnu/gstreamer-1.0`

```bash
export GST_PLUGIN_PATH=${GST_PLUGIN_PATH}:/usr/lib/x86_64-linux-gnu/gstreamer-1.0
```
