#!/bin/sh

set -ex

MAKE_JOBS=4

#TODO gcc:12
for compiler in gcc:9 gcc:10 gcc:11
do
    docker build -t mqmgateway-${compiler} --build-arg=COMPILER=${compiler} .
    docker run -e MAKE_JOBS=${MAKE_JOBS} -it -v $(pwd)/..:/src mqmgateway-${compiler}
done



