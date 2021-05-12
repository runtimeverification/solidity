ARG KIELE_COMMIT
FROM runtimeverificationinc/runtimeverification-iele-semantics:ubuntu-focal-${KIELE_COMMIT}

RUN    apt update        \
    && apt upgrade --yes \
    && apt install --yes \
       build-essential   \
       cmake             \
       gcovr             \
       libboost-all-dev  \
       libudev-dev       \
       libusb-1.0-0      \
       libxml2-utils     \
       libz3-dev         \
       llvm-11           \
       llvm-11-dev       \
       npm

ARG USER_ID=1000
ARG GROUP_ID=1000
RUN    groupadd --gid $GROUP_ID user \
    && useradd --create-home --uid $USER_ID --shell /bin/sh --gid user user

USER $USER_ID:$GROUP_ID

RUN    git config --global user.email 'admin@runtimeverification.com' \
    && git config --global user.name  'RV Jenkins'                    \
    && mkdir -p ~/.ssh                                                \
    && echo 'host github.com'                       > ~/.ssh/config   \
    && echo '    hostname github.com'              >> ~/.ssh/config   \
    && echo '    user git'                         >> ~/.ssh/config   \
    && echo '    identityagent SSH_AUTH_SOCK'      >> ~/.ssh/config   \
    && echo '    stricthostkeychecking accept-new' >> ~/.ssh/config   \
    && chmod go-rwx -R ~/.ssh
