ARG ALPINE_VERSION=latest
FROM alpine:${ALPINE_VERSION} AS builder
RUN apk update && apk add --no-cache \
      git build-base cmake pkgconf boost-dev libmodbus-dev mosquitto-dev yaml-cpp-dev rapidjson-dev catch2

ARG EXPRTK_URL=https://github.com/ArashPartow/exprtk/raw/master/exprtk.hpp
RUN if [ "${EXPRTK_URL-}" ]; then \
      mkdir -p /usr/local/include && \
      wget -O /usr/local/include/exprtk.hpp "${EXPRTK_URL}" || true; \
    fi

COPY . /opt/mqmgateway/source
ARG MQM_TEST_SKIP
RUN cmake \
      -DCMAKE_INSTALL_PREFIX:PATH=/opt/mqmgateway/install \
      ${MQM_TEST_SKIP:+-DWITHOUT_TESTS=1} \
      -S /opt/mqmgateway/source \
      -B /opt/mqmgateway/build
WORKDIR /opt/mqmgateway/build
RUN make -j$(nproc)
ARG MQM_TEST_ALLOW_FAILURE=false
ARG MQM_TEST_DEFAULT_WAIT_MS
ARG MQM_TEST_LOGLEVEL=3
RUN if [ -z "${MQM_TEST_SKIP}" ]; then cd unittests; ./tests || [ "${MQM_TEST_ALLOW_FAILURE}" = "true" ]; fi
RUN make install

FROM alpine:${ALPINE_VERSION} AS runtime
COPY --from=builder /opt/mqmgateway/install/ /usr/
COPY --from=builder /opt/mqmgateway/source/modmqttd/config.template.yaml /etc/modmqttd/config.yaml
RUN apk update && apk add --no-cache \
  $(apk search -e boost*-log | grep -o '^boost.*-log') \
  $(apk search -e boost*-program_options | grep -o '^boost.*-program_options') \
  libmodbus mosquitto yaml-cpp && \
  apk cache purge
ENTRYPOINT [ "/usr/bin/modmqttd" ]
CMD [ "--config", "/etc/modmqttd/config.yaml" ]
