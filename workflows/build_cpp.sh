#!/usr/bin/env bash

set -ex

source_dir=${1}
build_dir=${2}
target=${3:-install}

export SANITIZER=${source_dir}
export SANITIZER_BUILD=${build_dir}/serialization-sanitizer
mkdir -p "$SANITIZER_BUILD"
cd "$SANITIZER_BUILD"
rm -Rf ./*

cmake -G "${CMAKE_GENERATOR:-Ninja}" \
      -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
      -DCMAKE_CXX_COMPILER="${CXX:-c++}" \
      -DCMAKE_C_COMPILER="${CC:-cc}" \
      -DCMAKE_EXE_LINKER_FLAGS="${CMAKE_EXE_LINKER_FLAGS:-}" \
      -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-}" \
      -DCMAKE_INSTALL_PREFIX="$SANITIZER_BUILD/install" \
      "$SANITIZER"

time cmake --build . --target "${target}"
