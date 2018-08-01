# Use the official Ubuntu image 
FROM ubuntu:16.04 as builder

# Build image. eg:
# $ docker build -t runtimeverification/sol2iele:0.0.3 .
# Tag image to latest. eg:
# $ docker tag runtimeverification/sol2iele:0.0.3 runtimeverification/sol2iele:latest
# Publish image. eg:
# $ docker push runtimeverification/sol2iele:0.0.3
# $ docker push runtimeverification/sol2iele:latest
# Docker hub: https://hub.docker.com/r/runtimeverification/sol2iele/
MAINTAINER runtimeverification "contact@runtimeverification.com"

# 0. Add user 
RUN adduser appuser --disabled-password --gecos "" --quiet
RUN passwd -d appuser &> /dev/null

# 1. Build IELE
RUN apt-get update 
RUN apt-get install git -y
RUN apt-get install make gcc maven curl openjdk-8-jdk flex opam pkg-config libmpfr-dev autoconf libtool pandoc build-essential libffi-dev -y
USER appuser
WORKDIR /home/appuser
RUN git clone https://github.com/runtimeverification/iele-semantics.git
RUN cd iele-semantics && git submodule update --init --recursive
USER root
RUN curl -sSL https://get.haskellstack.org/ | sh
USER appuser
WORKDIR /home/appuser
RUN cd iele-semantics/.build/secp256k1 && ./autogen.sh && ./configure --enable-module-recovery --enable-module-ecdh --enable-experimental && make 
USER root
WORKDIR /home/appuser/iele-semantics/.build/secp256k1
RUN make install # install secp256k1 from bitcoin-core
USER appuser
WORKDIR /home/appuser
RUN cd iele-semantics && make deps 
RUN cd iele-semantics && eval `opam config env` && make && make install

# 2. Build Solidity 2 IELE Compiler
USER root
RUN apt-get install sudo lsb-release -y
USER appuser
WORKDIR /home/appuser
RUN git clone https://github.com/runtimeverification/solidity.git
WORKDIR /home/appuser/solidity
RUN git checkout sol2iele
USER root
WORKDIR /home/appuser/solidity
RUN ./scripts/install_deps.sh
USER appuser
WORKDIR /home/appuser/solidity
RUN mkdir build
RUN cd build && cmake .. && make

FROM ubuntu:16.04 
# Set up user
RUN adduser appuser --disabled-password --gecos "" --quiet
RUN passwd -d appuser &> /dev/null

# Set up command server
## Install python and set up server
USER root
RUN apt-get update
RUN apt-get install python3 libllvm5.0 libz3-dev libgmp10 curl -y

# Copy IELE 
COPY --from=builder /home/appuser/.local/bin/ /home/appuser/.local/bin/

# Copy Solidity
COPY --from=builder /home/appuser/solidity/build/solc/isolc /home/appuser/.local/bin/isolc

RUN chown -R appuser:appuser /home/appuser/.local

USER appuser
ENV PATH "$PATH:/home/appuser/.local/bin"
ADD ./server.py /home/appuser/server.py

EXPOSE 8080
COPY . /docker
CMD [ "python3", "/home/appuser/server.py" ]