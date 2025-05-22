#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

# === CONFIG ===
IMAGE_NAME="$1"
CONTAINER_NAME="temp_extract_container"
DEBS_DESTINATION_PATH="./deb_packages"

# === USAGE ===
if [[ -z "$IMAGE_NAME" ]]; then
    echo "Usage: $0 <docker_image_with_debs>"
    exit 1
fi

# === RUN CONTAINER AND COPY FILES ===
echo "Running container to extract .deb files..."
docker run --name "$CONTAINER_NAME" "$IMAGE_NAME" bash -c '
    mkdir -p /debs_to_copy
    shopt -s nullglob
    files=(/intel-dlstreamer*.deb)
    if [ ${#files[@]} -eq 0 ]; then
        echo "No .deb files found in /"
        exit 1
    fi
    cp "${files[@]}" /debs_to_copy/
'
echo "Copying files from container to host..."
mkdir -p "$DEBS_DESTINATION_PATH"
docker cp "$CONTAINER_NAME:/debs_to_copy/." "$DEBS_DESTINATION_PATH"
echo "Cleaning up container..."
docker rm "$CONTAINER_NAME"
echo "Finished."
echo "Packages available in $DEBS_DESTINATION_PATH:"
ls "$DEBS_DESTINATION_PATH"

# === VERIFY .deb PACKAGES ===
echo "Verifying if .deb packages were successfully extracted..."
if ! compgen -G "$DEBS_DESTINATION_PATH/*.deb" > /dev/null; then
    echo "❌ No .deb packages found in $DEBS_DESTINATION_PATH"
    exit 1
fi

echo "✅ Extracted .deb packages to $DEBS_DESTINATION_PATH:"
ls -lh "$DEBS_DESTINATION_PATH"
