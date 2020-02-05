#!/bin/bash


set -euo pipefail
hash docker 2>/dev/null || { echo >&2 'Make sure Docker is installed'; exit 1;  }

set +eo pipefail
docker version | grep -q Server
ret=$?
set -eo pipefail

if [ $ret -ne 0 ]; then
  echo "Cannot connect to Docker server. Is it running? Do you have the right user permissions?"
  exit 1
fi

if [ -z "$1" ]; then
  echo "Docker tag parameter cannot be empty"
  exit 1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd packaging/docker-for-binary
docker build --tag "lbry/lbrycrd:$1" -f Dockerfile.Auto "$DIR"
echo "$DOCKER_PASSWORD" | docker login --username "$DOCKER_USERNAME" --password-stdin
docker push "lbry/lbrycrd:$1"
