FROM gabrielfrasantos/embedded-devcontainer-cpp:v7.2.0@sha256:5fe0d1ae0ab020db875964752d4e9e07e54718ea54733b5f7a8c81753cd48027

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && CURL_VER=$(apt-cache madison curl | awk 'NR==1 {print $3}') \
    && apt-get install -y --no-install-recommends curl="${CURL_VER}" \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && NODE_VER=$(apt-cache madison nodejs | awk 'NR==1 {print $3}') \
    && apt-get install -y --no-install-recommends nodejs="${NODE_VER}" \
    && rm -rf /var/lib/apt/lists/*

HEALTHCHECK NONE
