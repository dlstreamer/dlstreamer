# gstreamer-vaapi-patches

Intel® Deep Learning Streamer (Intel® DL Streamer) has a LGPL-licensed patch for gstreamer-vaapi plugins:

* dma-vaapiencode.patch \
Patch enables DMA memory input in VAAPI elements thus expanding its capabilities.

GStreamer has a `Peek VADisplay and VaSurface` patch for gstreamer-vaapi plugins:

* The [patch](https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/435.patch) is based on gstreamer-vaapi merge request [#435](https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/435). It allows other MIT-licences plugins to obtain `VADisplay` from `GstContext` created by VAAPI elements. It also allows to get `VASurfaceID` from `GstBuffer` created by VAAPI elements. This patch is required to enable VAAPI preprocessor of inference elements.

## How to use these patches

To build gstreamer-vaapi with these patches you have to proceed following steps:

1. Install *meson* and *ninja*. We need them to configure and build gstreamer-vaapi project

```sh
python3 -m pip install meson ninja
```

2. Create working directory:

```sh
mkdir -p $HOME/mygst-vaapi
```

3. Move into working directory

```sh
cd $HOME/mygst-vaapi
```

4. Download patches (usually, you can take last version of file from `master`-branch of Intel DL Streamer's repository)

```sh
curl -sSL https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/435.patch -o gst-vaapi-peek-vadisplay.patch
curl -sSL https://raw.githubusercontent.com/openvinotoolkit/dlstreamer_gst/master/patches/gstreamer-vaapi/dma-vaapiencode.patch -o dma-vaapiencode.patch
```

5. Download appropriate version of gstreamer-vaapi's sources (e.g. 1.18.4)

```sh
curl -sSL https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-1.18.4.tar.xz -o gstreamer-vaapi-1.18.4.tar.xz
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
patch -p1 < ../gst-vaapi-peek-vadisplay.patch
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

To enable VAAPI preprocessing backend you have to pass to cmake that configures `dl_streamer` following parameter `-DENABLE_VAAPI=ON`
