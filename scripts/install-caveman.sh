#!/usr/bin/env bash
# Installs Claude Code CLI and caveman without requiring curl or pre-installed Node.js.
# Needs: wget, bash, tar, sha256sum
# Tested on: Ubuntu 26.04 (devcontainer)
set -euo pipefail

NODEJS_VER="20.20.2"
NODE_SHA256="df770b2a6f130ed8627c9782c988fda9669fa23898329a61a871e32f965e007d"
NODE_TAR="node-v${NODEJS_VER}-linux-x64.tar.xz"
NODE_URL="https://nodejs.org/dist/v${NODEJS_VER}/${NODE_TAR}"

CLAUDE_CODE_VER="2.1.167"

CAVEMAN_COMMIT="63a91ecadbf4c4719a4602a5abb00883f9966034"
CAVEMAN_SHA256="8ddef49c15f089c26affed3c31d97142c683e1d37a1499ae557281ca09c2712c"
CAVEMAN_URL="https://raw.githubusercontent.com/JuliusBrussee/caveman/${CAVEMAN_COMMIT}/install.sh"

# ── Node.js ────────────────────────────────────────────────────────────────
if command -v node >/dev/null 2>&1; then
    NODE_MAJOR=$(node -p "process.versions.node.split('.')[0]")
    if [ "$NODE_MAJOR" -ge 18 ]; then
        echo "node $(node --version) already installed, skipping download."
    else
        echo "node $(node --version) is too old (need ≥18). Aborting." >&2
        exit 1
    fi
else
    echo "Downloading Node.js v${NODEJS_VER}..."
    wget -q --show-progress -O "/tmp/${NODE_TAR}" "${NODE_URL}"
    echo "${NODE_SHA256}  /tmp/${NODE_TAR}" | sha256sum --check
    echo "Extracting Node.js to /usr/local..."
    tar -xJf "/tmp/${NODE_TAR}" -C /usr/local --strip-components=1
    rm "/tmp/${NODE_TAR}"
    echo "node $(node --version) installed."
fi

# ── Claude Code CLI ────────────────────────────────────────────────────────
if command -v claude >/dev/null 2>&1; then
    echo "claude $(claude --version 2>/dev/null || true) already installed, skipping."
else
    echo "Installing Claude Code CLI v${CLAUDE_CODE_VER}..."
    npm install -g "@anthropic-ai/claude-code@${CLAUDE_CODE_VER}"
    echo "claude $(claude --version 2>/dev/null || true) installed."
fi

# ── caveman ────────────────────────────────────────────────────────────────
echo "Downloading caveman installer..."
wget -q -O /tmp/caveman-install.sh "${CAVEMAN_URL}"
echo "${CAVEMAN_SHA256}  /tmp/caveman-install.sh" | sha256sum --check
echo "Running caveman installer..."
bash /tmp/caveman-install.sh --non-interactive --only claude
rm /tmp/caveman-install.sh
echo "Done. Start any Claude Code session and say 'caveman mode', or run /caveman."
