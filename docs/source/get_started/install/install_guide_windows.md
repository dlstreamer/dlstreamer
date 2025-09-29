# Install Guide Windows

This page describes steps required to install Deep Learning Streamer Pipeline
Framework on Windows.

## On Windows System

### Step 1: Install GStreamer

Download and install the latest GStreamer from the
[GStreamer download page](https://gstreamer.freedesktop.org/download/#windows). The installation
directory should be `C:\\gstreamer`.

### Step 2: Download and extract DL Streamer dll files

Download the archive from
[DL Streamer assets on GitHub](https://github.com/open-edge-platform/edge-ai-libraries/releases)
Extract to a new folder, for example `C:\\dlstreamer_dlls`.

### Step 3: Run setup script

Open a PowerShell prompt and run

```bash
cd C:\\dlstreamer_dlls
.\setup_dls_env.ps1
```

You are ready to use Deep Learning Streamer. For further instructions to run
sample pipeline(s), please go to the [tutorial](../tutorial.md)
There is need to manually download models.

------------------------------------------------------------------------

> **\*** *Other names and brands may be claimed as the property of
> others.*
