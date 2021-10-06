#!/bin/sh
#
VERSION=$(cat Makefile | sed -e '/^[[:blank:]]*VERSION=/ ! d' -e 's/.*=//1')
MINORVERSION=$(git rev-list --count 9e113976bd3d67e830168b257754eb650c5ab6a4..HEAD)
COMMIT=$(git rev-parse --short HEAD)
COMMITMOD=$(test -z "`git status --porcelain -uno`" || echo "-modified")
FULLVERSION=${VERSION}.${MINORVERSION}-${COMMIT}${COMMITMOD}
#
echo ${VERSION} > .hos_version
echo ${MINORVERSION} > .hos_minor_version
echo ${FULLVERSION} > .hos_full_version
#
echo $FULLVERSION
