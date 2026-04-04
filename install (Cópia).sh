#!/bin/bash
# install.sh — Installation script for the BirdNET VAMP plugin.
#
# This script performs the following steps:
#   1. Downloads the Audacity 3.7.7 AppImage if not already present.
#   2. Verifies the AppImage integrity via SHA256 checksum.
#   3. Installs system build dependencies (cmake, g++, vamp-plugin-sdk).
#   4. Creates a Conda environment (birdnet-plugin) and installs the birdnet package.
#   5. Compiles the VAMP plugin into the build/ directory.
#   6. Copies birdnet_run.py into build/ alongside the plugin.
#   7. Creates a desktop shortcut named Audacity-BirdNet that launches the
#      bundled AppImage with VAMP_PATH pointing to build/.
#
# The AppImage is self-contained and does not interfere with any existing
# Audacity installation on the system.
#
# Requirements:
#   - Miniconda or Anaconda installed at ~/miniconda3
#   - Ubuntu 22.04 or compatible Debian-based system
#   - Internet connection for downloading the AppImage and Python packages
#
# Usage:
#   bash install.sh

set -e

REPO_DIR="$(realpath "$(dirname "$0")")"
VAMP_DIR="$REPO_DIR/build"
APPIMAGE_NAME="audacity-linux-3.7.7-x64-22.04.AppImage"
APPIMAGE_URL="https://github.com/audacity/audacity/releases/download/Audacity-3.7.7/$APPIMAGE_NAME"
APPIMAGE_PATH="$REPO_DIR/$APPIMAGE_NAME"
APPIMAGE_SHA256="45c4445fb6670cc5fe40d31c7cea979724d2605bca53b554c32520acbf901ef0"
CONDA_ENV="birdnet-plugin"
CONDA_PYTHON="$HOME/miniconda3/envs/$CONDA_ENV/bin/python3"

# ── Create required directories ───────────────────────────────────────────────
mkdir -p "$HOME/.local/share/applications"

# ── Download Audacity AppImage if not present ─────────────────────────────────
if [ ! -f "$APPIMAGE_PATH" ]; then
    echo "==> Downloading Audacity AppImage..."
    wget -q --show-progress -O "$APPIMAGE_PATH" "$APPIMAGE_URL"
else
    echo "==> Audacity AppImage already exists, skipping download."
fi

# ── Verify AppImage integrity (SHA256) ────────────────────────────────────────
echo "==> Verifying AppImage integrity..."
ACTUAL_SHA256=$(sha256sum "$APPIMAGE_PATH" | awk '{print $1}')
if [ "$ACTUAL_SHA256" != "$APPIMAGE_SHA256" ]; then
    echo "ERROR: AppImage integrity check failed!"
    echo "  Expected: $APPIMAGE_SHA256"
    echo "  Got:      $ACTUAL_SHA256"
    echo "  The file may be corrupted or tampered with. Deleting it."
    rm -f "$APPIMAGE_PATH"
    exit 1
fi
chmod +x "$APPIMAGE_PATH"
echo "    Integrity check passed."

# ── Install system build dependencies ────────────────────────────────────────
echo ""
echo "==> Installing system dependencies..."
sudo apt-get update -qq
sudo apt-get install -y cmake g++ vamp-plugin-sdk

# ── Create Conda environment and install birdnet ──────────────────────────────
echo ""
echo "==> Setting up Conda environment '$CONDA_ENV'..."
source "$HOME/miniconda3/etc/profile.d/conda.sh"

if conda env list | grep -q "^$CONDA_ENV "; then
    echo "    Environment '$CONDA_ENV' already exists, skipping creation."
else
    conda create -y -n "$CONDA_ENV" python=3.12
fi

conda run -n "$CONDA_ENV" pip install birdnet --quiet

# ── Compile the VAMP plugin ───────────────────────────────────────────────────
echo ""
echo "==> Compiling VAMP plugin..."
rm -rf "$VAMP_DIR"
mkdir -p "$VAMP_DIR" && cd "$VAMP_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd "$REPO_DIR"

# ── Copy inference script to build/ ──────────────────────────────────────────
cp "$REPO_DIR/birdnet_run.py" "$VAMP_DIR/"

# ── Create desktop shortcut ───────────────────────────────────────────────────
echo ""
echo "==> Creating Audacity-BirdNet desktop shortcut..."
cat > "$HOME/.local/share/applications/audacity-birdnet.desktop" << DESKTOP
[Desktop Entry]
Name=Audacity-BirdNet
Comment=Audacity Audio Editor with BirdNET Plugin
Exec=env VAMP_PATH=$VAMP_DIR $APPIMAGE_PATH %F
Icon=audacity
Terminal=false
Type=Application
Categories=Audio;AudioVideo;
DESKTOP
update-desktop-database "$HOME/.local/share/applications/" 2>/dev/null || true

echo ""
echo "Installation complete!"
echo "Python interpreter: $CONDA_PYTHON"
echo ""
echo "Launch Audacity-BirdNet from the application menu or run:"
echo "  VAMP_PATH=$PWD/build ./$APPIMAGE_NAME"
echo ""
echo "Inside Audacity:"
echo "  1. Open an audio file"
echo "  2. Go to Analyze -> BirdNET"
echo "  3. Detections will appear as labeled regions on the track"
