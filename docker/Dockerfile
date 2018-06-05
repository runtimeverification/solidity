# Use the official Ubuntu image 
FROM ubuntu:16.04

# Build image. eg:
# $ docker build -t runtimeverification/sol2iele:0.0.3 .
# Tag image to latest. eg:
# $ docker tag runtimeverification/sol2iele:0.0.3 runtimeverification/sol2iele:latest
# Publish image. eg:
# $ docker push runtimeverification/sol2iele:0.0.3
# $ docker push runtimeverification/sol2iele:latest
# Docker hub: https://hub.docker.com/r/runtimeverification/sol2iele/
MAINTAINER runtimeverification "contact@runtimeverification.com"

# TODO: Build IELE and Solidity to IELE Compiler.

# 1. Build IELE
RUN apt-get update 
RUN apt-get install git -y
RUN git clone https://github.com/runtimeverification/iele-semantics.git
RUN apt-get install make gcc maven curl openjdk-8-jdk flex opam pkg-config libmpfr-dev autoconf libtool pandoc build-essential libffi-dev -y
RUN cd iele-semantics && git submodule update --init --recursive
RUN curl -sSL https://get.haskellstack.org/ | sh
RUN cd iele-semantics/.build/secp256k1 && ./autogen.sh && ./configure --enable-module-recovery --enable-module-ecdh --enable-experimental && make && make install # install secp256k1 from bitcoin-core
RUN cd iele-semantics && make deps 
RUN cd iele-semantics && eval `opam config env` && make && make install

# 2. Build Solidity 2 IELE Compiler
RUN apt-get install sudo lsb-release -y
RUN git clone https://github.com/runtimeverification/solidity.git
RUN cd solidity && git checkout sol2iele
RUN cd solidity && ./scripts/install_deps.sh
RUN cd solidity && mkdir build
RUN cd solidity && cd build && cmake .. && make

# Set environment variable
ENV PATH "$PATH:/root/.local/bin:/solidity/build/solc"

# Set up command server
## Install python and set up server
RUN apt-get install python3 -y

EXPOSE 8080
# Copy file
## docker run -tp 127.0.0.1:6666:8080 runtimeverification/sol2iele:latest
##                
COPY . /docker
CMD [ "python3", "/docker/server.py" ]