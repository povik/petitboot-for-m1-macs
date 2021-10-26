FROM debian:latest

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update -y
RUN apt-get install -y --no-install-recommends \
	git build-essential fakeroot libncurses5-dev libssl-dev ccache \
	dfu-util u-boot-tools device-tree-compiler mtools \
	bc python cpio zip unzip rsync file wget \
	gcc-arm-linux-gnueabihf python3 python


RUN echo PS1=\"\\\w % \" >> /etc/bash.bashrc

