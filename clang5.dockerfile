
FROM ubuntu:18.04 as base

ARG proxy="http://wwwproxy.sandia.gov:80/"
ARG compiler=clang-5.0

ENV https_proxy=${proxy} \
    http_proxy=${proxy}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y -q && \
    apt-get install -y -q --no-install-recommends \
    clang-5.0 \
    libclang-5.0-dev \
    clang-tools-5.0 \
    llvm-5.0 \
    llvm-5.0-dev \
    cmake \
    git \
    less \
    ninja-build \
    ca-certificates \
    valgrind \
    wget \
    curl \
    ccache \
    make-guile

RUN ln -s \
    "$(which $(echo ${compiler}  | cut -d- -f1)++-$(echo ${compiler}  | cut -d- -f2))" \
    /usr/bin/clang++

ENV CC=${compiler} \
    CXX=clang++

FROM base as build
COPY . /serialization-sanitizer

RUN ln -s \
    /usr/bin/llvm-config-5.0 \
    /usr/bin/llvm-config

RUN /serialization-sanitizer/build.sh /serialization-sanitizer /build

ENTRYPOINT ["/build/serialization-sanitizer/checker"]
