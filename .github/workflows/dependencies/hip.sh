#!/usr/bin/env bash
#
# Copyright 2020 The WarpX Community
#
# License: BSD-3-Clause-LBNL
# Authors: Axel Huebl

set -eu -o pipefail

# `man apt.conf`:
#   Number of retries to perform. If this is non-zero APT will retry
#   failed files the given number of times.
echo 'Acquire::Retries "3";' | sudo tee /etc/apt/apt.conf.d/80-retries

# Ref.: https://rocm.docs.amd.com/projects/install-on-linux/en/latest/how-to/native-install/ubuntu.html

# Make the directory if it doesn't exist yet.
# This location is recommended by the distribution maintainers.
sudo mkdir --parents --mode=0755 /etc/apt/keyrings

# Download the key, convert the signing-key to a full
# keyring required by apt and store in the keyring directory
wget https://repo.radeon.com/rocm/rocm.gpg.key -O - | \
    gpg --dearmor | sudo tee /etc/apt/keyrings/rocm.gpg > /dev/null

curl -O https://repo.radeon.com/rocm/rocm.gpg.key
sudo apt-key add rocm.gpg.key

source /etc/os-release # set UBUNTU_CODENAME: focal or jammy or ...

VERSION=${1-6.3.2}

echo "deb [arch=amd64] https://repo.radeon.com/rocm/apt/${VERSION} ${UBUNTU_CODENAME} main" \
  | sudo tee /etc/apt/sources.list.d/rocm.list
echo 'export PATH=/opt/rocm/llvm/bin:/opt/rocm/bin:/opt/rocm/profiler/bin:/opt/rocm/opencl/bin:$PATH' \
  | sudo tee -a /etc/profile.d/rocm.sh

# we should not need to export HIP_PATH=/opt/rocm/hip with those installs

sudo apt-get update

# Ref.: https://rocmdocs.amd.com/en/latest/Installation_Guide/Installation-Guide.html#installing-development-packages-for-cross-compilation
# meta-package: rocm-dkms
# OpenCL: rocm-opencl
# other: rocm-dev rocm-utils
sudo apt-get install -y --no-install-recommends \
    build-essential \
    gfortran        \
    libhiredis-dev  \
    libnuma-dev     \
    libopenmpi-dev  \
    libzstd-dev     \
    ninja-build     \
    openmpi-bin     \
    rocm-dev${VERSION}        \
    roctracer-dev${VERSION}   \
    rocprofiler-dev${VERSION} \
    rocrand-dev${VERSION}     \
    rocfft-dev${VERSION}      \
    rocprim-dev${VERSION}     \
    rocsparse-dev${VERSION}

# hiprand-dev is a new package that does not exist in old versions
sudo apt-get install -y --no-install-recommends hiprand-dev${VERSION} || true

# ccache
$(dirname "$0")/ccache.sh

# activate
#
source /etc/profile.d/rocm.sh
hipcc --version
which clang
which clang++
export CXX=$(which clang++)
export CC=$(which clang)

# cmake-easyinstall
#
sudo curl -L -o /usr/local/bin/cmake-easyinstall https://raw.githubusercontent.com/ax3l/cmake-easyinstall/main/cmake-easyinstall
sudo chmod a+x /usr/local/bin/cmake-easyinstall
export CEI_SUDO="sudo"
export CEI_TMP="/tmp/cei"
