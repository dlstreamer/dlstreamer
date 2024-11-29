#!/bin/bash
# ==============================================================================
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

REPO_USER=${1}
REPO_PASS=${2}
REPO_URL=${3}
PACKAGE_DIR=${4}
DISTRIBUTION=${5}
COMPONENT_NAME=${6}
EDSS_USER=${7}
EDSS_PASS=${8}
CA_PATH=${9}

error() {
    printf '::error::%s %s\n' "$1" "$2" >&2
    exit 1
}

[ -z "$REPO_USER" ] && error "Repository username is not provided"
[ -z "$REPO_PASS" ] && error "Repository password is not provided"
[ -z "$REPO_URL" ] && error "Repository URL is not provided"
[ -z "$DISTRIBUTION" ] && error "Debian distribution is not provided"
[ -z "$COMPONENT_NAME" ] && error "Debian component name is not provided"

pushd ${PACKAGE_DIR}

echo "::group::Collecting packages"
deb_packs=$(ls | grep ".deb") || error "There are no Debian packages in " ${PACKAGE_DIR}
echo "$deb_packs"
echo "::endgroup::"

# Stop on first error.
set -e

echo "::group::Signing packages"
echo "CA file: ${CA_PATH}"
if [ -z "$EDSS_USER" ] || [ -z "${EDSS_PASS}" ]; then
    echo "::notice::Skipping signing deb packages because EDDS username/password is not provided"
else
    SignFile -vv -cafile ${CA_PATH} -u ${EDSS_USER} -p ${EDSS_PASS} *.deb
fi
echo "::endgroup::"

delete_old_packages() {
    echo "::group::Deleting old packages"
    echo "Repo URL: ${REPO_URL}"
    for pack in $deb_packs; do
        pack_name=$(basename "$pack")
        base_name="${pack_name%_ubuntu_22.04_amd64.deb}"
        echo "Deleting old package: ${base_name}_amd64.deb"
        curl -u${REPO_USER}:${REPO_PASS} -XDELETE "${REPO_URL}/${base_name}_amd64.deb${arti_params}"
        echo "Deleting old package: ${base_name}_ubuntu_22.04_amd64.deb"
        curl -u${REPO_USER}:${REPO_PASS} -XDELETE "${REPO_URL}/${base_name}_ubuntu_22.04_amd64.deb${arti_params}"
        echo "Deleting old package: ${base_name}_ubuntu_24.04_amd64.deb"
        curl -u${REPO_USER}:${REPO_PASS} -XDELETE "${REPO_URL}/${base_name}_ubuntu_24.04_amd64.deb${arti_params}"
    done
    echo "::endgroup::"
}

# Call the function to delete old packages
delete_old_packages

echo "::group::Uploading"
arti_params=";deb.distribution=${DISTRIBUTION};deb.component=${COMPONENT_NAME};deb.architecture=amd64"
echo "Repo URL: ${REPO_URL}"
echo "Artifactory params: ${arti_params}"
for pack in $deb_packs; do
    curl --upload-file "$pack" -u${REPO_USER}:${REPO_PASS} -XPUT "${REPO_URL}/$pack${arti_params}"
done
echo "::endgroup::"

popd
