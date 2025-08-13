# Ubuntu advanced uninstall guide

## Option #1: Uninstall Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework from APT repository

If you installed via apt package just simple uninstall with apt

```bash
sudo dpkg-query -l | awk '/^ii/ && /intel-dlstreamer/ {print $2}' | sudo xargs apt-get remove -y --purge
```

## Option #2: Intel® DL Streamer Pipeline Framework Docker image

If you used docker, you need just remove container and dlstreamer docker
image

```bash
docker rm <container-name>
docker rmi dlstreamer:devel
```

## Option #3: Compiled version

If you compiled from sources, follow by this instructions

### Step 1: Uninstall Intel® DL Streamer

```bash
sudo apt-get remove intel-dlstreamer-gst libpython3-dev python-gi-dev libopencv-dev libva-dev
```

Uninstall Python dependencies

```bash
cd ~/intel/dlstreamer_gst
sudo python3 -m pip uninstall -r requirements.txt
```

### Step 2: Uninstall optional components

> **NOTE:** If you haven't installed any optional/additional components, you can
> skip this step.

Uninstall OpenCL™

```bash
sudo apt remove intel-opencl-icd ocl-icd-opencl-dev opencl-clhpp-headers
```

Uninstall Intel® oneAPI DPC++/C++ Compiler

```bash
sudo apt remove intel-oneapi-compiler-dpcpp-cpp intel-level-zero-gpu level-zero-dev
```

Uninstall VA-API dependencies

```bash
sudo apt remove intel-dlstreamer-gst-vaapi libva-dev vainfo intel-media-va-driver-non-free
```

### Step 3: Remove source directory

```bash
rm -rf ~/intel/dlstreamer_gst
```

------------------------------------------------------------------------

> **\*** *Other names and brands may be claimed as the property of
> others.*
