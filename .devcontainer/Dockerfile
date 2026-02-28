FROM gabrielfrasantos/embedded-devcontainer-cpp:latest@sha256:9393ef6a1ded67410b5dac0e80999bedc32bd6c4c145f7d1c7ba098e06192987

HEALTHCHECK NONE

RUN apt-get update && apt-get install -y --no-install-recommends \
    gnuplot-qt \
    qt6-base-dev \
    libgl1-mesa-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-0 \
    libxcb-xinerama0 \
    libxcb-cursor0 \
    && rm -rf /var/lib/apt/lists/*
