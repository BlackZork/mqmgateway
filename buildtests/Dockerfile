ARG COMPILER=gcc:9
FROM ${COMPILER}
ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /

# Debian bullseye (the base of the gcc 9 to 11 images) is past its support
# period, and deb.debian.org no longer serves all of its packages; the complete
# set, security updates included, is on archive.debian.org
RUN . /etc/os-release \
   && if [ "${VERSION_CODENAME}" = "bullseye" ]; then \
        sed -i 's|http://deb.debian.org/|http://archive.debian.org/|' /etc/apt/sources.list; \
      fi

# deps install
# catch2 v3 from sources for older images
RUN apt-get update \
   && apt-get install -y cmake rapidjson-dev libfmt-dev libyaml-cpp-dev libmosquitto-dev pkgconf libspdlog-dev libmodbus-dev gfortran- \
   && wget https://github.com/catchorg/Catch2/archive/refs/tags/v3.6.0.tar.gz \
   && tar xf v3.6.0.tar.gz \
   && cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -S /Catch2-3.6.0 -B /catch2-build \
   && make -j $(nproc) -C /catch2-build install \
   && rm -rf v3.6.0.tar.gz /Catch2-3.6.0 /catch2-build \
   && rm -rf /var/lib/apt/lists/*

# every exprtk.hpp listed in exprtk-versions, as /opt/exprtk/<declared date>/exprtk.hpp;
# a header that does not declare the date on its line is the wrong commit
COPY exprtk-versions /opt/exprtk/exprtk-versions
RUN grep -v '^#' /opt/exprtk/exprtk-versions | while read -r date commit; do \
      [ -n "${date}" ] || continue; \
      mkdir -p /opt/exprtk/${date} \
      && wget -q -O /opt/exprtk/${date}/exprtk.hpp https://raw.githubusercontent.com/ArashPartow/exprtk/${commit}/exprtk.hpp \
      && grep -q "date *= *\"${date}\"" /opt/exprtk/${date}/exprtk.hpp \
      || { echo "exprtk ${commit} does not declare ${date}"; exit 1; }; \
    done

# start build from src. Cmdline:
# docker run -it -v $(pwd)/..:/src mqmgateway-gcc:9
# add -e EXPRTK_MATRIX=1 to also test every exprtk header, see build.sh;
# add -e MAKE_JOBS=<n> to build with fewer parallel jobs than all cores
VOLUME /src
WORKDIR /src
CMD ["sh", "-c", "/src/buildtests/build.sh"]