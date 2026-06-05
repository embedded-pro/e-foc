FROM gabrielfrasantos/embedded-devcontainer-cpp:v7.2.0@sha256:5fe0d1ae0ab020db875964752d4e9e07e54718ea54733b5f7a8c81753cd48027

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG CURL_VER
RUN apt-get update \
    && : "${CURL_VER:?Set CURL_VER to an explicit apt version (see: apt-cache madison curl)}" \
    && apt-get install -y --no-install-recommends curl="${CURL_VER}" \
    && rm -rf /var/lib/apt/lists/*

ARG NODEJS_VER
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get update \
    && : "${NODEJS_VER:?Set NODEJS_VER to an explicit apt version (see: apt-cache madison nodejs)}" \
    && apt-get install -y --no-install-recommends nodejs="${NODEJS_VER}" \
    && rm -rf /var/lib/apt/lists/*

HEALTHCHECK NONE
