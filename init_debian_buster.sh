#!/bin/bash

set -ex

# buster is out-of-support, point the apt lists at the archive
sed -i s/deb.debian.org/archive.debian.org/g /etc/apt/sources.list
sed -i s/security.debian.org/archive.debian.org/g /etc/apt/sources.list

# First, enable multiarch in dpkg/apt land
dpkg --add-architecture i386

# Get both updated amd64 and 'new to us' i386 packages
apt-get update
apt-get upgrade -y

# 64-bit tools.  The debian:buster image has very, very old packages in it.
# The 'git' it includes does not support the recursive git submodule mode of
# the various checkout actions so update it to 2.18 or better.
apt-get install -y git jq python3-pip gettext bison mawk

# Multiarch tools and libraries.  Get the compiler and library tooling needed
# for i386 and amd64 building.
apt-get install -y gcc-8-multilib gcc-8-base:amd64 gcc-8-base:i386 libgcc-8-dev:amd64 libgcc-8-dev:i386 libgcc1:amd64 libgcc1:i386
apt-get install -y libglib2.0-0:amd64 libglib2.0-0:i386 libglib2.0-bin libglib2.0-data libglib2.0-dev:amd64 libglib2.0-dev:i386 libglib2.0-dev-bin
apt-get install -y libstdc++-8-dev:amd64 libstdc++-8-dev:i386 libstdc++6:amd64 libstdc++6:i386 zlib1g:amd64 zlib1g:i386 zlib1g-dev:amd64 zlib1g-dev:i386
apt-get install -y build-essential libpthread-stubs0-dev clang-11
apt-get install -y gcc-multilib g++-multilib cmake

# Finally, autobuild
pip3 --no-cache-dir install pydot==1.4.2 pyzstd==0.15.10 autobuild

update-alternatives --install /usr/bin/cc cc /usr/bin/clang-11 100
update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-11 100

cd /root
git clone https://github.com/secondlife/build-variables.git
cd build-variables
# Use the same branch as the github action
git checkout universal
echo 'export AUTOBUILD_VARIABLES_FILE=/root/build-variables/variables' >> ~/.bashrc
