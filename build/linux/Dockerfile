# Build environment for the Linux native host package.
# Ubuntu 22.04 keeps glibc old enough for the package to run on Ubuntu 22.04+ and Debian 12+.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential debhelper devscripts fakeroot pkg-config \
      qtbase5-dev qt5-qmake qtchooser libpcsclite-dev python3 \
 && rm -rf /var/lib/apt/lists/*
