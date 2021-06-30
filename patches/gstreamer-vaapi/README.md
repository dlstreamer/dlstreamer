# gstreamer-vaapi-patches

There are three LGPL-licensed patches for gstreamer-vaapi plugins:

* vasurface_qdata.patch \
Patch allows other MIT-licences plugins to extract VASurfaceID and VADisplay from GstBuffer created by VAAPI plugins. Which is meant to enable VA-API preprocessor of inference elements.

* video-format-mapping.patch \
Patch to improve the mapping between VA and GST formats. The new map will be generated dynamically, based on the query result of ab image format in VA driver. Also consider the ambiguity of RGB color format in LSB mode.

* dma-vaapiencode.patch \
Patch enables DMA memory input in VAAPI elements thus expanding its capabilities.

## How to use these patches

To build gstreamer-vaapi with these patches you have to proceed following steps:

1. Install *meson* and *ninja*. We need them to configure and build gstreamer-vaapi project

```sh
pip3 install meson ninja
```

2. Create working directory:

```sh
mkdir -p $HOME/mygst-vaapi
```

3. Move into working directory

```sh
cd $HOME/mygst-vaapi
```

4. Download patches (usually, you can take last version of file from `master`-branch of DL Streamer's repository)

```sh
wget https://raw.githubusercontent.com/opencv/gst-video-analytics/master/patches/gstreamer-vaapi/vasurface_qdata.patch
wget https://raw.githubusercontent.com/opencv/gst-video-analytics/master/patches/gstreamer-vaapi/dma-vaapiencode.patch
wget https://raw.githubusercontent.com/opencv/gst-video-analytics/master/patches/gstreamer-vaapi/video-format-mapping.patch
```

5. Download appropriate version of gstreamer-vaapi's sources (e.g. 1.18.4)

```sh
wget https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-1.18.4.tar.xz
```

6. Unpack downloaded archive

```sh
tar xf gstreamer-vaapi-1.18.4.tar.xz
```

7. Move into unpacked directory

```sh
cd gstreamer-vaapi-1.18.4
```

8. Apply patches

```sh
patch -p1 < ../vasurface_qdata.patch
patch -p1 < ../video-format-mapping.patch
patch -p1 < ../dma-vaapiencode.patch
```

9. Configure for building gstreamer-vaapi with applied patches (disable examples, docs and test because we don't need them)

```sh
meson \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dgtk_doc=disabled \
    build/
```

> **NOTE**: If you are experiencing troubles configuring gstreamer-vaapi consider rebooting machine if you haven't done it after installing OpenVINO™ Toolkit. Otherwise:
> ```sh
> sudo apt install libudev-dev
> ```
> * If the libva dependency cannot be found:
> ```sh
> export PKG_CONFIG_PATH=/opt/intel/mediasdk/lib64/pkgconfig:$PKG_CONFIG_PATH
> ```
> * If you are facing the error `Neither a subproject directory nor a gst-plugins-bad.wrap file was found` install the gstreamer-plugins-bad developer package:
> ```sh
> sudo apt install libgstreamer-plugins-bad1.0-dev
> ```
> Reconfigure gstreamer-vaapi after the steps above.

10. Build configured gstreamer-vaapi project

```sh
ninja -C build/
```

11. Install into desired directory (let's use `$HOME/mygst-vaapi/install` for example):

```sh
mkdir -p $HOME/mygst-vaapi/install
DESTDIR=$HOME/mygst-vaapi/install meson install -C build/
```

12. As output of previous command you should see path to installed *libgstvaapi.so* library. depending on your system this path may be different, as example we use path that most likely you will see in Ubuntu.
To start using this library you have to add directory containing that *libgstvaapi.so* to your `GST_PLUGIN_PATH` **as leading path**:

```sh
export GST_PLUGIN_PATH=$HOME/mygst-vaapi/install/usr/local/lib/x86_64-linux-gnu/gstreamer-1.0/:$GST_PLUGIN_PATH
```

13. Available VA-API plugins can be shown by:

```sh
gst-inspect-1.0 vaapi
```

> **NOTE**: If the library is found but the command above shows `0 features` refer to the note section in step 9.
> Then repeat steps 9, 10, 11 to reconfigure your gstreamer-vaapi.

## VAAPI preprocessing backend

To enable VAAPI preprocessing backend you have to pass to cmake that configures `gst-video-analytics` following parameter `-DENABLE_VAAPI=ON`
