#!/bin/sh
# Builds the tree with each compiler image below, then, with the last one, runs
# the unit tests against every exprtk header in exprtk-versions (see build.sh).
# Run from this directory. Builds use every core; set MAKE_JOBS to use fewer.

set -ex

COMPILERS="gcc:9 gcc:10 gcc:11 gcc:12 gcc:13 gcc:14"
# the exprtk matrix runs once, with the last (newest) compiler
MATRIX_COMPILER=${COMPILERS##* }

# an interactive terminal gets one, so that Ctrl-C reaches the build
TTY=
if [ -t 0 ]; then
    TTY=-it
fi

for compiler in ${COMPILERS}
do
    matrix=0
    if [ "${compiler}" = "${MATRIX_COMPILER}" ]; then
        matrix=1
    fi
    docker build -t mqmgateway-${compiler} --build-arg=COMPILER=${compiler} .
    docker run --rm ${TTY} ${MAKE_JOBS:+-e MAKE_JOBS=${MAKE_JOBS}} -e EXPRTK_MATRIX=${matrix} -v $(pwd)/..:/src mqmgateway-${compiler}
done
