# gstreamer-vaapi-patch

LGPL-licensed patch for gstreamer-vaapi plugins to allow other MIT-licences plugins to extract VASurfaceID and VADisplay from GstBuffer created by VAAPI plugins. This patch is meant to enable VA-API preprocessor of inference elements.

## How to use this patch

To build gstreamer-vaapi with this patch you have to proceed following steps:

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

4. Download patch (usually, you can take last version of file from `master`-branch of DL Streamer's repository)

```sh
wget https://raw.githubusercontent.com/opencv/gst-video-analytics/master/patches/gstreamer-vaapi/vasurface_qdata.patch
```

5. Download appropriate version of gstreamer-vaapi's sources (e.g. 1.16.2)

```sh
wget https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-1.16.2.tar.xz
```

6. Unpack downloaded archive

```sh
tar xf gstreamer-vaapi-1.16.2.tar.xz
```

7. Move into unpacked directory

```sh
cd gstreamer-vaapi-1.16.2
```

8. Apply patch

```sh
patch -p1 < ../vasurface_qdata.patch
```

9. Configure for building gstreamer-vaapi with applied patch (disable examples, docs and test because we don't need them)

```sh
meson \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dgtk_doc=disabled \
    build/
```

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
To start using this library you have to add directory containing that *libgstvaapi.so` to your `GST_PLUGIN_PATH` **as leading path**:

```sh
export GST_PLUGIN_PATH=$HOME/mygst-vaapi/install/usr/local/lib/x86_64-linux-gnu/gstreamer-1.0/:$GST_PLUGIN_PATH
```

## VAAPI preprocessing backend

To enable VAAPI preprocessing backend you have to pass to cmake that configures `gst-video-analytics` following parameter `-DENABLE_VAAPI=ON`
