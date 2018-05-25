#!/usr/bin/env sh

set -e

docker login -u="$DOCKER_USERNAME" -p="$DOCKER_PASSWORD";
version=$($(dirname "$0")/get_version.sh)
if [ "$TRAVIS_BRANCH" = "develop" ]
then
    docker tag ethereum/isolc:build ethereum/isolc:nightly;
    docker tag ethereum/isolc:build ethereum/isolc:nightly-"$version"-"$TRAVIS_COMMIT"
    docker push ethereum/isolc:nightly-"$version"-"$TRAVIS_COMMIT";
    docker push ethereum/isolc:nightly;
elif [ "$TRAVIS_TAG" = v"$version" ]
then
    docker tag ethereum/isolc:build ethereum/isolc:stable;
    docker tag ethereum/isolc:build ethereum/isolc:"$version";
    docker push ethereum/isolc:stable;
    docker push ethereum/isolc:"$version";
else
    echo "Not publishing docker image from branch $TRAVIS_BRANCH or tag $TRAVIS_TAG"
fi
