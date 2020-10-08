#!/usr/bin/env bash

set -ex

source_dir=${1}
build_dir=${2}

export CH=${source_dir}
export CH_BUILD=${build_dir}/checkpoint-member-analyzer
mkdir -p "$CH_BUILD"
cd "$CH_BUILD"
rm -Rf ./*

target=install

cmake -G "${CMAKE_GENERATOR:-Ninja}" "$CH"
time cmake --build . --target "${target}"
