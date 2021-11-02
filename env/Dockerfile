FROM runtimeverificationinc/runtimeverification-iele-semantics:ubuntu-focal-06ee8bbb7c57

RUN    apt-get update        \
    && apt-get upgrade --yes \
    && apt-get install --yes \
       build-essential   \
       cmake             \
       curl              \
       libboost-all-dev  \
       libudev-dev       \
       libusb-1.0-0      \
       libxml2-utils     \
       libz3-dev         \
       llvm-11           \
       llvm-11-dev       \
       openjdk-11-jdk

ARG USER_ID=1000
ARG GROUP_ID=1000
RUN    groupadd --gid $GROUP_ID user \
    && useradd --create-home --uid $USER_ID --shell /bin/sh --gid user user

USER $USER_ID:$GROUP_ID
WORKDIR /home/user

ARG USERNAME
ARG TOKEN

RUN wget https://github.com/sbt/sbt/releases/download/v1.5.5/sbt-1.5.5.tgz
RUN tar xvf sbt-1.5.5.tgz
ENV PATH="/home/user/sbt/bin:${PATH}"

RUN mkdir -p solc/bin
WORKDIR /home/user/solc/bin
RUN wget https://github.com/ethereum/solidity/releases/download/v0.8.2/solc-static-linux -O solc
RUN chmod a+x solc
ENV PATH="/home/user/solc/bin:${PATH}"

WORKDIR /home/user
RUN git clone https://$USERNAME:$TOKEN@github.com/input-output-hk/midnight.git
WORKDIR /home/user/midnight
RUN git checkout checking-for-pending-transactions-after-tx-build
RUN sbt dist
WORKDIR /home/user
RUN unzip midnight/node/instance/target/universal/midnight-node-v0.19.3.zip
RUN unzip midnight/wallet/backend/target/universal/midnight-wallet-v0.19.3.zip

COPY node-patch /home/user/midnight-node-v0.19.3/conf
COPY wallet-patch /home/user/midnight-wallet-v0.19.3/conf
WORKDIR /home/user/midnight-node-v0.19.3/conf
RUN patch -p1 < node-patch
WORKDIR /home/user/midnight-wallet-v0.19.3/conf
RUN patch -p1 < wallet-patch

WORKDIR /home/user
RUN git clone https://github.com/runtimeverification/solidity.git
WORKDIR /home/user/solidity
RUN git checkout midnight
RUN mkdir build && cd build && cmake .. && make -j`nproc`

WORKDIR /home/user
COPY init.sh soltest.sh qa_mineBlocks.json wallet_restore.json wallet_start.json /home/user/
ENV PATH="/usr/lib/kiele/:${PATH}"
