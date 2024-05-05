#/bin/bash

set -ex

mkdir -p /build
cmake -B /build
cd /build
make -j ${MAKE_JOBS}