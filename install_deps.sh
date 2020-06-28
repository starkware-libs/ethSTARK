#!/bin/bash

# Show executed shell commands
set -o xtrace
# Exit on error.
set -e

apt update
apt install -y cmake python3.7 git wget gnupg2 elfutils libdw-dev python3-pip

pip3 install cpplint pytest numpy

# Install clang-6.0.
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
apt update
apt install -y software-properties-common
apt-add-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-6.0 main"
apt-add-repository ppa:ubuntu-toolchain-r/test -y
apt update

apt install -y clang-6.0
ln -sf /usr/bin/clang++-6.0 /usr/bin/clang++
ln -sf /usr/bin/clang-6.0 /usr/bin/clang

# Install gtest.
(
cd /tmp
rm -rf /tmp/googletest
git clone -b release-1.8.0 https://github.com/google/googletest.git
cd /tmp/googletest
cmake CMakeLists.txt && make -j && make install
)

# Install gflags.
(
cd /tmp
rm -rf /tmp/gflags
git clone -b v2.2.1 https://github.com/gflags/gflags.git
cd /tmp/gflags
cmake CMakeLists.txt && make -j && make install
)

# Install glog.
(
cd /tmp
rm -rf /tmp/glog
git clone -b v0.3.5 https://github.com/google/glog.git
cd glog
cmake CMakeLists.txt && make -j && make install
)

# Install google benchmark.
(
cd /tmp
rm -rf /tmp/benchmark
git clone -b v1.4.0 https://github.com/google/benchmark.git
cd /tmp/benchmark
cmake CMakeLists.txt -DCMAKE_BUILD_TYPE=Release && make -j && make install
)
