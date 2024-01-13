#!/bin/bash

set -e #-x

ver_pattern='^v[0-9]+\.[0-9]+\.[0-9]+$'
version=$(git describe --tags)

if [[ ! $version =~ $ver_pattern ]]; then
    echo "Repository not clean: ${version}"
    exit 1;
fi

mkdir -p release
git archive HEAD --output release/${version}.src.zip
git archive HEAD --output release/${version}.src.tgz
