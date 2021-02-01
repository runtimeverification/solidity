#!/usr/bin/env bash
set -e

ROOTDIR="$(dirname "$0")/.."
BUILDDIR="${ROOTDIR}/build"

if [[ $# -eq 0 ]]; then
    BUILD_TYPE=Release
else
    BUILD_TYPE="$1"
fi

if [[ "$(git tag --points-at HEAD 2>/dev/null)" == v* ]]; then
	touch "${ROOTDIR}/prerelease.txt"
fi

mkdir -p "${BUILDDIR}"
cd "${BUILDDIR}"

cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "${@:2}"
make -j2

<<<<<<< ours
if [ -z $CI ]; then
	echo "Installing isolc and soltest"
	install solc/isolc /usr/local/bin && install test/soltest /usr/local/bin
=======
if [[ "${CI}" == "" ]]; then
	echo "Installing ..."
	sudo make install
>>>>>>> theirs
fi
