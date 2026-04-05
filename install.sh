#!/bin/bash
# install.sh — Installation script for the BirdNET VAMP plugin.
#
# This script performs the following steps:
#   1. Installs system build dependencies (cmake, g++, vamp-plugin-sdk).
#   2. Creates a Conda environment (birdnet-plugin) and installs the birdnet package.
#   3. Compiles the VAMP plugin into the build/ directory.
#   4. Copies birdnet_run.py into build/ alongside the plugin.
#   5. Copies compiled plugin and .py files to ~/vamp.
#
# Requirements:
#   - Miniconda or Anaconda installed at ~/miniconda3
#   - Ubuntu 22.04 or compatible Debian-based system
#   - Internet connection for downloading the AppImage and Python packages
#
# Usage:
#   bash install.sh or ./install.sh

set -e

REPO_DIR="$(realpath "$(dirname "$0")")"
VAMP_DIR="$REPO_DIR/build"
CONDA_ENV="birdnet-plugin"
CONDA_PYTHON="$HOME/miniconda3/envs/$CONDA_ENV/bin/python3"

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

# ── Copy plugin + Python files to ~/vamp ─────────────────────────────────────
echo ""
echo "==> Copying plugin files to $HOME/vamp..."
mkdir -p "$HOME/vamp"
cp "$VAMP_DIR"/birdnet-vamp.so "$HOME/vamp/"
cp "$VAMP_DIR"/birdnet_run.py "$HOME/vamp/"

echo ""
echo "Installation complete!"
echo "Python interpreter: $CONDA_PYTHON"
echo ""
echo "To launch Audacity-BirdNet execute:"
echo "  ./audacity.sh"
echo ""
echo "To launch Sonic-Visualiser-BirdNet execute:"
echo "  ./sonic-visualiser.sh"
