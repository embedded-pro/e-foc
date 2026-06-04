FROM gabrielfrasantos/embedded-devcontainer-cpp:v7.2.0@sha256:5fe0d1ae0ab020db875964752d4e9e07e54718ea54733b5f7a8c81753cd48027

RUN apt-get update && apt-get install -y --no-install-recommends curl && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

HEALTHCHECK NONE
