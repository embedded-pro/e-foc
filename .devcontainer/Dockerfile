FROM gabrielfrasantos/embedded-devcontainer-cpp:latest@sha256:52930380ffae6dbb5df2e9ee8366d9458e4a8dcb951dc2aa48cbec90acb4a8ba

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

RUN xwin --accept-license splat --include-debug-libs --preserve-ms-arch-notation --output /winsdk \
    && rm -rf /root/.xwin-cache

RUN pip3 install --break-system-packages --no-cache-dir aqtinstall==3.3.0 \
    && aqt install-qt windows desktop 6.4.2 win64_msvc2019_64 --outputdir /opt/qt6-windows \
    && pip3 uninstall --break-system-packages -y aqtinstall \
    && rm -rf /tmp/aqt*
