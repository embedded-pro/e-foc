FROM gabrielfrasantos/embedded-devcontainer-cpp:v7.2.0@sha256:5fe0d1ae0ab020db875964752d4e9e07e54718ea54733b5f7a8c81753cd48027

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# hadolint ignore=DL3008
RUN --mount=type=bind,source=.devcontainer/apt-requirements.json,target=/tmp/apt-requirements.json \
    apt-get update \
    && apt-get install -y --no-install-recommends jq \
    && jq -r 'to_entries | .[] | .key + "=" + .value' /tmp/apt-requirements.json \
       | xargs apt-get install -y --no-install-recommends \
    && rm -rf /var/lib/apt/lists/*

ARG NODEJS_VER
ARG NODE_SHA256
ADD --checksum=sha256:${NODE_SHA256} https://nodejs.org/dist/v${NODEJS_VER}/node-v${NODEJS_VER}-linux-x64.tar.xz /tmp/node.tar.xz
RUN : "${NODEJS_VER:?Set NODEJS_VER to an explicit Node.js version (see: https://nodejs.org/dist)}" \
    && : "${NODE_SHA256:?Set NODE_SHA256 to the sha256 of node-v\${NODEJS_VER}-linux-x64.tar.xz (see: https://nodejs.org/dist/v\${NODEJS_VER}/SHASUMS256.txt)}" \
    && tar -xJf /tmp/node.tar.xz -C /usr/local --strip-components=1 \
    && rm /tmp/node.tar.xz

ARG CAVEMAN_COMMIT=63a91ecadbf4c4719a4602a5abb00883f9966034
ARG CAVEMAN_SHA256=8ddef49c15f089c26affed3c31d97142c683e1d37a1499ae557281ca09c2712c
ADD --checksum=sha256:${CAVEMAN_SHA256} https://raw.githubusercontent.com/JuliusBrussee/caveman/${CAVEMAN_COMMIT}/install.sh /tmp/caveman-install.sh
RUN bash /tmp/caveman-install.sh \
    && rm /tmp/caveman-install.sh

HEALTHCHECK NONE
