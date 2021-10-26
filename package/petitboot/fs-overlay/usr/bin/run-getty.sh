#!/bin/sh

setsid /usr/sbin/agetty -n -a root "$1" &
