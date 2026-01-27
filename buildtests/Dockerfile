ARG COMPILER=gcc:9
FROM ${COMPILER}
ENV DEBIAN_FRONTEND noninteractive
ENV MAKE_JOBS=4

WORKDIR /

# deps install
# catch2 v3 from sources for older images
RUN apt-get update \
   && apt-get install -y cmake rapidjson-dev libfmt-dev libyaml-cpp-dev libmosquitto-dev pkgconf libspdlog-dev libmodbus-dev gfortran- \
   && wget https://github.com/catchorg/Catch2/archive/refs/tags/v3.6.0.tar.gz \
   && tar xf v3.6.0.tar.gz \
   && cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -S /Catch2-3.6.0 -B /catch2-build \
   && make -j ${MAKE_JOBS} -C /catch2-build install \
   && rm -rf v3.6.0.tar.gz /Catch2-3.6.0 /catch2-build \
   && rm -rf /var/lib/apt/lists/*

RUN 


# start build from src. Cmdline:
# docker run -e MAKE_JOBS=16 -it -v $(pwd)/..:/src mqmgateway-9
VOLUME /src
WORKDIR /src
CMD ["sh", "-c", "/src/buildtests/build.sh"]