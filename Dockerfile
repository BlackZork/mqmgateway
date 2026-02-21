ARG ALPINE_VERSION=3.23
FROM alpine:${ALPINE_VERSION} AS builder
RUN apk update && apk add --no-cache \
      git build-base cmake pkgconf spdlog-dev libmodbus-dev mosquitto-dev yaml-cpp-dev rapidjson-dev catch2-3

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
ARG MQM_TEST_TIMING_FACTOR
ARG MQM_TEST_LOGLEVEL=info
RUN if [ -z "${MQM_TEST_SKIP}" ]; then cd unittests; ./tests || [ "${MQM_TEST_ALLOW_FAILURE}" = "true" ]; fi
RUN make install

FROM alpine:${ALPINE_VERSION} AS runtime
COPY --from=builder /opt/mqmgateway/install/ /usr/
COPY --from=builder /opt/mqmgateway/source/modmqttd/config.template.yaml /etc/modmqttd/config.yaml
RUN apk update && apk add --no-cache \
  spdlog libmodbus mosquitto yaml-cpp && \
  apk cache purge
ENTRYPOINT [ "/usr/bin/modmqttd" ]
CMD [ "--config", "/etc/modmqttd/config.yaml" ]
