FROM gabrielfrasantos/embedded-devcontainer-cpp:latest@sha256:7a175a091628c1b74de2a0570e49948050f11320eba8709e647ad480a4928086

HEALTHCHECK NONE

RUN apt-get update && apt-get install -y --no-install-recommends \
    gnuplot-qt=6.0.0+dfsg1-1ubuntu3 \
    qt6-base-dev=6.4.2+dfsg-21.1build5 \
    libgl1-mesa-dev=25.2.8-0ubuntu0.24.04.1 \
    libxkbcommon-dev=1.6.0-1build1 \
    libxkbcommon-x11-0=1.6.0-1build1 \
    libxcb-xinerama0=1.15-1ubuntu2 \
    libxcb-cursor0=0.1.4-1build1 \
    && rm -rf /var/lib/apt/lists/*

RUN xwin --accept-license splat --include-debug-libs --preserve-ms-arch-notation --output /winsdk \
    && rm -rf /root/.xwin-cache

RUN pip3 install --break-system-packages --no-cache-dir aqtinstall==3.3.0 \
    && aqt install-qt windows desktop 6.4.2 win64_msvc2019_64 --outputdir /opt/qt6-windows \
    && pip3 uninstall --break-system-packages -y aqtinstall \
    && rm -rf /tmp/aqt*
