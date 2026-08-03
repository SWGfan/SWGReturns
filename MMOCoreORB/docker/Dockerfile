# Build from the repo root, e.g.:
#   docker build -f MMOCoreORB/docker/Dockerfile -t core3 .
#
# Verified building on Ubuntu 24.04 (Noble); should carry forward to 26.04 LTS.
# debian:jessie is EOL and no longer has a usable package set for this project.
# This image builds and runs THIS repo's own source (previously it cloned
# unrelated upstream projects instead).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get upgrade -y && apt-get install -y \
	build-essential \
	cmake \
	git \
	ccache \
	pkg-config \
	default-jre \
	liblua5.3-dev \
	libdb5.3-dev \
	libssl-dev \
	libmariadb-dev \
	libmariadb-dev-compat \
	libboost-dev \
	libboost-thread-dev \
	supervisor \
	&& rm -rf /var/lib/apt/lists/*

ADD . /Core3/

WORKDIR /Core3/MMOCoreORB/

RUN mkdir -p build && cd build \
	&& cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	&& make -j"$(nproc)"

RUN sed -i 's#/git/Core3/MMOCoreORB#/Core3/MMOCoreORB#' docker/scripts/supervisord-core3.conf
RUN mkdir -p /etc/supervisor/conf.d && cp docker/scripts/supervisord-core3.conf /etc/supervisor/conf.d/supervisord-core3.conf

# The MySQL/MariaDB server the game connects to is expected to run as a
# separate service/container (e.g. via docker-compose), not inside this image.
# Configure it in MMOCoreORB/bin/conf/config.lua before starting.

CMD ["supervisord", "-n"]
