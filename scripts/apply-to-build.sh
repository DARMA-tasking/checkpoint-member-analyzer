#!/usr/bin/env bash

set -ex

sanitizer_install=${1}
target_source_dir=${2}
target_build_dir=${3}

compile_commands="${target_build_dir}/compile_commands.json"

if test -f "${compile_commands}"
then
    echo "Found ${compile_commands}";
else
    echo "Could not find compile_commands.json in ${target_build_dir}"
    exit 1;
fi

sanitizer="${sanitizer_install}/bin/sanitizer"
parse_json="${sanitizer_install}/scripts/parse-json.pl"

if test -f ${sanitizer}
then
    echo "Found sanitizer binary: ${sanitizer}"
else
    echo "Could not find sanitizer binary: ${sanitizer}"
    exit 2;
fi

if test -f ${parse_json}
then
    echo "Found sanitizer parse json script: ${parse_json}"
else
    echo "Could not find parse json script: ${parse_json}"
    exit 3;
fi

${parse_json} 0 ${sanitizer} ${compile_commands} \.\* 'examples'
${parse_json} 0 ${sanitizer} ${compile_commands} \.\* 'tests/unit' 'main.cc'
