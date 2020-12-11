#!/usr/bin/env bash

set -ex

sanitizer=$1
commands=$2
file=$3

t=$(mktemp)
echo ${sanitizer} -p ${commands} -include-input ${file} > $t
