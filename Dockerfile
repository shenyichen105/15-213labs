FROM ubuntu:14.04
MAINTAINER Yichen Shen
RUN apt-get update
RUN apt-get install -y software-properties-common
RUN apt-get install -y gcc
RUN apt-get install -y gdb
RUN apt-get install -y valgrind
RUN apt-get install -y make
RUN \
  apt-get update && \
  apt-get install -y python python-dev python-pip python-virtualenv && \
  rm -rf /var/lib/apt/lists/*
RUN mkdir /15213
WORKDIR /15213
VOLUME /15213

