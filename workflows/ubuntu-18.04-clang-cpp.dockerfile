
FROM ubuntu:18.04 as base

ARG proxy=""
ARG compiler=clang-5.0

ENV https_proxy=${proxy} \
    http_proxy=${proxy}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y -q && \
    apt-get install -y -q --no-install-recommends \
    ${compiler} \
    lib${compiler}-dev \
    clang-tools-$(echo ${compiler} | cut -d- -f2) \
    llvm-$(echo ${compiler} | cut -d- -f2) \
    llvm-$(echo ${compiler} | cut -d- -f2)-dev \
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

RUN ln -s \
    /usr/bin/llvm-config-$(echo ${compiler} | cut -d- -f2) \
    /usr/bin/llvm-config

ENV CC=${compiler} \
    CXX=clang++

FROM base as build
COPY . /serialization-sanitizer

RUN /serialization-sanitizer/workflows/build_cpp.sh /serialization-sanitizer /build

ENTRYPOINT ["/build/serialization-sanitizer/sanitizer"]
