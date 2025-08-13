# Ubuntu advanced installation - build Docker image

The easiest way to run DLStreamer in Docker is to pull docker images
from DockerHub. If you would like to follow this way, please go to
Option 2 in
[the installation guide](../../get_started/install/install_guide_ubuntu).

The instruction below shows how to build Docker images based on
Ubuntu22/24 from DLStreamer Dockerfiles.

## Step 1: Install prerequisites

Please go through Prerequisites described in
[the installation guide](../../get_started/install/install_guide_ubuntu).

## Step 2: Download Dockerfiles

You can download these files from the main repository using commands
below:

All Dockerfiles are in
[DLStreamer GitHub repository](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/docker).

### Ubuntu24 debian/dev Dockerfile

```bash
wget https://raw.githubusercontent.com/open-edge-platform/edge-ai-libraries/main/libraries/dl-streamer/docker/ubuntu/ubuntu24.Dockerfile
```

### Ubuntu22 debian/dev Dockerfile

```bash
wget https://raw.githubusercontent.com/open-edge-platform/edge-ai-libraries/main/libraries/dl-streamer/docker/ubuntu/ubuntu22.Dockerfile
```

## Step 3: Build Docker image

This is a template command line which builds a Docker image from the
specified Dockerfile using the current directory as the build context

```bash
docker build -f <Dockerfile name> -t <name for Docker image> .
```

This command builds a Docker debian image from the
**ubuntu22.Dockerfile** assigning it the name **dlstreamer-ubuntu22**
using the current directory as the build context

```bash
docker build -f ubuntu22.Dockerfile -t dlstreamer-ubuntu22 .
```

This command builds a Docker development image from the
**ubuntu22.Dockerfile** assigning it the name
**dlstreamer-dev-ubuntu22** using the current directory as the build
context

```bash
docker build --target dlstreamer-dev -f ubuntu22.Dockerfile -t dlstreamer-dev-ubuntu22 .
```
