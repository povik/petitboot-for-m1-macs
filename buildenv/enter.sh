#!/bin/sh -e

THISDIR=$(cd $(dirname $0) && pwd)
PRJROOT=${THISDIR%/*}
IMAGE=petitboot-buildroot-buildenv
docker build $THISDIR --tag $IMAGE
docker run --rm -it \
  --user 1000:1000 \
  -v /etc/ssl/certs/ca-certificates.crt:/etc/ssl/certs/ca-certificates.crt:ro \
  -v $PRJROOT:/base -w /base $IMAGE $@

