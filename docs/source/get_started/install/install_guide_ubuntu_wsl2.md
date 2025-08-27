# Install Guide Ubuntu 24.04 on WSL2

Page describes steps required to install Deep Learning Streamer Pipeline
Framework on host Windows machine and Ubuntu on WSL2

## On Windows Host System

### Step 1: Update the GPU drivers

Download and install the latest Intel® GPU drivers from
[intel-arc-iris-xe-graphics-windows](https://www.intel.com/content/www/us/en/download/785597/intel-arc-iris-xe-graphics-windows.html)

### Step 2: Install WSL

Open a PowerShell prompt as an Administrator and run

```bash
wsl --install
```

or

```bash
wsl --update
```

in case you have installed it before.

Visit
[install-ubuntu-wsl2](https://documentation.ubuntu.com/wsl/en/latest/howto/install-ubuntu-wsl2)
in case of any Ubuntu-WSL2 installation issues.

### Step 3: Install Ubuntu 24.04.1 LTS

Open a PowerShell prompt as an Administrator and run

```bash
wsl --install Ubuntu-24.04
wsl --set-default Ubuntu-24.04
```

## On Linux WSL System - setup proxy (optional steps)

### Step 1: Edit /etc/bash.bashrc and append lines with http_proxy and https_proxy

```bash
export http_proxy=""
export https_proxy=""
```

### Step 2: Run command

```bash
sudo visudo
```

After line:

```bash
Defaults        env_reset
```

Add line:

```bash
Defaults        env_keep = "http_proxy https_proxy"
```

### Step 3: Run command

```bash
source /etc/profile
```

## On Linux WSL System

### Step 1: Check if /dev/dri directory is present, output should look similar to this

```bash
ls -ltrah /dev/dri

total 0
drwxr-xr-x  3 root root        100 Mar 24 16:00 .
crw-rw----  1 root render 226, 128 Mar 24 16:00 renderD128
crw-rw----  1 root video  226,   0 Mar 24 16:00 card0
drwxr-xr-x  2 root root         80 Mar 24 16:00 by-path
drwxr-xr-x 16 root root       3.5K Mar 24 16:00 ..
```

### Step 2: If you don't have renderD12\* device, please update GPU drivers and update WSL

### Step 3: Install GPU libraries

```bash
/wget -qO - https://repositories.intel.com/graphics/intel-graphics.key |   sudo gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg
echo 'deb [arch=amd64,i386 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu noble arc' |   sudo tee  /etc/apt/sources.list.d/intel.gpu.noble.list
sudo apt update
sudo apt-get install -y  libze-dev intel-opencl-icd  intel-media-va-driver-non-free libmfx1  libvpl2   libegl-mesa0 libegl1-mesa-dev libgbm1 libgl1-mesa-dev libgl1-mesa-dri   libglapi-mesa libgles2-mesa-dev libglx-mesa0 libigdgmm12 libxatracker2 mesa-va-drivers   mesa-vdpau-drivers mesa-vulkan-drivers va-driver-all
```

### Step 4: Add OpenVINO™ Toolkit and Deep Learning Streamer repositories

```bash
sudo -E wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
sudo wget -O- https://eci.intel.com/sed-repos/gpg-keys/GPG-PUB-KEY-INTEL-SED.gpg | sudo tee /usr/share/keyrings/sed-archive-keyring.gpg > /dev/null
sudo echo "deb [signed-by=/usr/share/keyrings/sed-archive-keyring.gpg] https://eci.intel.com/sed-repos/$(source /etc/os-release && echo $VERSION_CODENAME) sed main" | sudo tee /etc/apt/sources.list.d/sed.list
sudo bash -c 'echo -e "Package: *\nPin: origin eci.intel.com\nPin-Priority: 1000" > /etc/apt/preferences.d/sed'
sudo bash -c 'echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/openvino/2025 ubuntu24 main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2025.list'
```

### Step 5: Install Deep Learning Streamer Pipeline Framework

```bash
sudo apt update
sudo apt-get install intel-dlstreamer
```

### Step 6: Test by running hello_dlstreamer script

#### CPU device

```bash
/opt/intel/dlstreamer/scripts/hello_dlstreamer.sh
```

#### GPU device

```bash
/opt/intel/dlstreamer/scripts/hello_dlstreamer.sh --device=GPU
```

------------------------------------------------------------------------

> **\*** *Other names and brands may be claimed as the property of
> others.*
