#!/usr/bin/env bash

set -exo pipefail

source_dir=${1}
build_dir=${2}

export SANITIZER=${source_dir}
export SANITIZER_BUILD=${build_dir}/serialization-sanitizer
pushd "$SANITIZER_BUILD"

ctest --output-on-failure | tee cmake-output.log

popd
