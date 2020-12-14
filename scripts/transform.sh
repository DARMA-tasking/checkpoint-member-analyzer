#!/usr/bin/env bash

set -ex

sanitizer=$1
commands=$2
file=$3

t=$(mktemp)
${sanitizer} -p "${commands}" -include-input "${file}" > "$t"
cp "${file}" "${file}.bak"
cp "${t}" "${file}"
