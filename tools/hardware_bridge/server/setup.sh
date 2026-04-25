#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- 1. Check / Install Python -------------------------------------------
echo "Checking Python installation..."

if command -v python3 &>/dev/null; then
    echo "Python found: $(python3 --version)"
else
    echo "Python not found. Installing Python..."

    if command -v apt-get &>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y python3 python3-pip
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y python3 python3-pip
    elif command -v yum &>/dev/null; then
        sudo yum install -y python3 python3-pip
    elif command -v pacman &>/dev/null; then
        sudo pacman -Sy --noconfirm python python-pip
    elif command -v zypper &>/dev/null; then
        sudo zypper install -y python3 python3-pip
    else
        echo "ERROR: Unsupported package manager. Please install Python 3 manually." >&2
        exit 1
    fi

    if ! command -v python3 &>/dev/null; then
        echo "ERROR: Python installation failed." >&2
        exit 1
    fi

    echo "Python installed successfully: $(python3 --version)"
fi

# --- 2. Create / Use virtual environment ---------------------------------
VENV_DIR="$SCRIPT_DIR/.venv"
VENV_PYTHON="$VENV_DIR/bin/python"

if [ ! -x "$VENV_PYTHON" ]; then
    echo "Creating virtual environment at $VENV_DIR..."
    if ! python3 -m venv "$VENV_DIR"; then
        echo "ERROR: Could not create a virtual environment." >&2
        echo "Install the Python venv package for your distribution (for example: sudo apt-get install python3-venv) and rerun setup.sh." >&2
        exit 1
    fi
fi

echo "Using Python virtual environment: $($VENV_PYTHON --version)"

# --- 3. Install requirements into the virtual environment -----------------
REQUIREMENTS="$SCRIPT_DIR/requirements.txt"

if [ -f "$REQUIREMENTS" ]; then
    echo "Installing requirements from requirements.txt..."
    "$VENV_PYTHON" -m pip install --upgrade pip --quiet
    "$VENV_PYTHON" -m pip install -r "$REQUIREMENTS"
    echo "Requirements installed successfully."
else
    echo "WARNING: requirements.txt not found at $REQUIREMENTS - skipping."
fi

# --- 4. Add server folder to PATH if not already present -----------------
echo "Checking if server folder is in PATH..."

PROFILE_FILE=""
if [ -f "$HOME/.bashrc" ]; then
    PROFILE_FILE="$HOME/.bashrc"
elif [ -f "$HOME/.bash_profile" ]; then
    PROFILE_FILE="$HOME/.bash_profile"
elif [ -f "$HOME/.profile" ]; then
    PROFILE_FILE="$HOME/.profile"
fi

case ":$PATH:" in
    *":$SCRIPT_DIR:"*)
        echo "Folder is already in PATH."
        ;;
    *)
        echo "Adding '$SCRIPT_DIR' to PATH..."
        if [ -n "$PROFILE_FILE" ]; then
            echo "" >> "$PROFILE_FILE"
            echo "# Added by setup.sh" >> "$PROFILE_FILE"
            echo "export PATH=\"\$PATH:$SCRIPT_DIR\"" >> "$PROFILE_FILE"
            export PATH="$PATH:$SCRIPT_DIR"
            echo "Folder added to PATH in $PROFILE_FILE. Run 'source $PROFILE_FILE' or restart your shell."
        else
            echo "WARNING: Could not find a shell profile file. Add the following line manually:" >&2
            echo "  export PATH=\"\$PATH:$SCRIPT_DIR\"" >&2
        fi
        ;;
esac

echo ""
echo "Setup complete."
echo "Run the server with: $VENV_PYTHON $SCRIPT_DIR/bridge_server.py"
echo "Or activate the environment with: source $VENV_DIR/bin/activate"
