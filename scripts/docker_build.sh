#!/usr/bin/env sh

set -e

docker build -t ethereum/isolc:build -f scripts/Dockerfile .
tmp_container=$(docker create ethereum/isolc:build sh)
mkdir -p upload
docker cp ${tmp_container}:/usr/bin/isolc upload/isolc-static-linux
