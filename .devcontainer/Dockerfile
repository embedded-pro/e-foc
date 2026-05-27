FROM gabrielfrasantos/embedded-devcontainer-cpp:latest@sha256:52930380ffae6dbb5df2e9ee8366d9458e4a8dcb951dc2aa48cbec90acb4a8ba

HEALTHCHECK NONE

RUN apt-get update && apt-get install -y --no-install-recommends \
    gnuplot-qt=6.0.2+dfsg1-2ubuntu2 \
    qt6-base-dev=6.10.2+dfsg-7 \
    libgl1-mesa-dev=26.0.3-1ubuntu1 \
    libxkbcommon-dev=1.13.1-1 \
    libxkbcommon-x11-0=1.13.1-1 \
    libxcb-xinerama0=1.17.0-2ubuntu1 \
    libxcb-cursor0=0.1.6-1 \
    && rm -rf /var/lib/apt/lists/*

RUN xwin --accept-license splat --include-debug-libs --preserve-ms-arch-notation --output /winsdk \
    && rm -rf /root/.xwin-cache

RUN pip3 install --break-system-packages --no-cache-dir aqtinstall==3.3.0 \
    && aqt install-qt windows desktop 6.4.2 win64_msvc2019_64 --outputdir /opt/qt6-windows \
    && pip3 uninstall --break-system-packages -y aqtinstall \
    && rm -rf /tmp/aqt*
