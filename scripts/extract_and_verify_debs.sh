#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

# === CONFIG ===
IMAGE_NAME="$1"
DISTRIBUTION="${2:-ubuntu}"
CONTAINER_NAME="temp_extract_container"

# === USAGE ===
if [[ -z "$IMAGE_NAME" ]]; then
    echo "Usage: $0 <docker_image_with_debs> <optional:ubuntu/fedora default:ubuntu>"
    exit 1
fi

if [[ "$DISTRIBUTION" == "ubuntu" ]]; then
    PKGS_DESTINATION_PATH="./deb_packages"
    PKG=".deb"
    PKGS="debs"
elif [[ "$DISTRIBUTION" == "fedora" ]]; then
    PKGS_DESTINATION_PATH="./rpm_packages"
    PKG=".rpm"
    PKGS="rpms"
else
    echo "Unsupported distribution: $DISTRIBUTION. Use 'ubuntu' or 'fedora'."
    exit 1
fi

# === RUN CONTAINER AND COPY FILES ===
echo "Running container to extract $PKG files..."
docker run --name "$CONTAINER_NAME" -e PKG="$PKG" -e PKGS="$PKGS" "$IMAGE_NAME" bash -c '
    shopt -s nullglob
    files=(/${PKGS}/intel-dlstreamer*${PKG})
    if [ ${#files[@]} -eq 0 ]; then
        echo "No $PKG files found in /"
        exit 1
    fi
'
echo "Copying files from container to host..."
mkdir -p "$PKGS_DESTINATION_PATH"
docker cp "$CONTAINER_NAME:/$PKGS/." "$PKGS_DESTINATION_PATH"
echo "Cleaning up container..."
docker rm "$CONTAINER_NAME"
echo "Finished."
echo "Packages available in $PKGS_DESTINATION_PATH:"
ls "$PKGS_DESTINATION_PATH"

# === VERIFY .deb PACKAGES ===
echo "Verifying if $PKG packages were successfully extracted..."
if ! compgen -G "$PKGS_DESTINATION_PATH/*$PKG" > /dev/null; then
    echo "❌ No $PKG packages found in $PKGS_DESTINATION_PATH"
    exit 1
fi

echo "✅ Extracted $PKG packages to $PKGS_DESTINATION_PATH:"
ls -lh "$PKGS_DESTINATION_PATH"
