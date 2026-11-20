#!/bin/bash
# Builds the tree mounted at /src with this image's compiler, against the newest
# exprtk header in /opt/exprtk.
#
# With EXPRTK_MATRIX=1 it then switches the same build to every header there
# and runs the unit tests for each. Only exprconv includes exprtk.hpp, so a
# switch rebuilds that plugin alone. A header dated before EXPRTK_MIN_DATE in
# CMakeLists.txt must instead make configure fail.
#
# make runs one job per core unless MAKE_JOBS says otherwise.

set -ex

JOBS=${MAKE_JOBS:-$(nproc)}
BUILD=/build
EXPRTK_DIR=/opt/exprtk
MIN_DATE=$(sed -n 's/^set(EXPRTK_MIN_DATE \([0-9]*\))$/\1/p' /src/CMakeLists.txt)
if [ -z "${MIN_DATE}" ]; then
    echo "EXPRTK_MIN_DATE not found in CMakeLists.txt"
    exit 1
fi

DATES=$(cd ${EXPRTK_DIR} && ls -d */ | tr -d / | sort -n)
NEWEST=$(echo "${DATES}" | tail -n 1)

mkdir -p ${BUILD}
cmake -S /src -B ${BUILD} -DEXPRTK_INCLUDE_DIR=${EXPRTK_DIR}/${NEWEST}
make -C ${BUILD} -j ${JOBS}

if [ "${EXPRTK_MATRIX:-0}" != "1" ]; then
    exit 0
fi

for date in ${DATES}; do
    if [ "${date}" -lt "${MIN_DATE}" ]; then
        # a scratch build dir, so that a refused configure leaves ${BUILD} alone
        if cmake -S /src -B /build-refused -DEXPRTK_INCLUDE_DIR=${EXPRTK_DIR}/${date} > /tmp/refused.log 2>&1; then
            echo "exprtk ${date} is older than ${MIN_DATE} but configure accepted it"
            exit 1
        fi
        # CMake wraps long error messages, so match on the log as one line
        tr -s '\n ' '  ' < /tmp/refused.log | grep -q "exprconv needs ${MIN_DATE} or newer"
        continue
    fi
    cmake -S /src -B ${BUILD} -DEXPRTK_INCLUDE_DIR=${EXPRTK_DIR}/${date}
    make -C ${BUILD} -j ${JOBS}
    (cd ${BUILD}/unittests && MQM_TEST_TIMING_FACTOR=${MQM_TEST_TIMING_FACTOR:-10} ./tests)
done
