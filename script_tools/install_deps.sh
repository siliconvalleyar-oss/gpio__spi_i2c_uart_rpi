#!/usr/bin/env bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

ok()   { echo -e " ${GREEN}[OK]${NC} $1"; }
warn() { echo -e " ${YELLOW}[..]${NC} $1"; }
fail() { echo -e " ${RED}[FAIL]${NC} $1"; }

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_DIR"

echo "============================================"
echo " GPIO Generator - Dependency Installer"
echo "============================================"
echo ""

# ---- apt packages ----
warn "Updating package list..."
sudo apt-get update -qq
ok "Package list updated"

DEPS="build-essential git rsync sshpass"
warn "Installing: $DEPS"
sudo apt-get install -y -qq $DEPS
ok "System packages installed"

# ---- bcm2835 library ----
warn "Checking bcm2835 library..."

if grep -q 'BCM2835_VERSION' /usr/local/include/bcm2835.h 2>/dev/null; then
    VER=$(grep 'BCM2835_VERSION' /usr/local/include/bcm2835.h | grep -oP '\d+')
    ok "bcm2835 already installed (v${VER:0:1}.${VER:1:2})"
else
    warn "bcm2835 not found. Installing from source..."

    BCM_VER="1.77"
    BCM_URL="http://www.airspayce.com/mikem/bcm2835/bcm2835-${BCM_VER}.tar.gz"
    TMPDIR=$(mktemp -d)
    cd "$TMPDIR"

    warn "Downloading bcm2835-${BCM_VER}..."
    if wget -q "$BCM_URL" -O "bcm2835-${BCM_VER}.tar.gz"; then
        tar xzf "bcm2835-${BCM_VER}.tar.gz"
        cd "bcm2835-${BCM_VER}"
        warn "Configuring..."
            ./configure --quiet 2>/dev/null
        warn "Building..."
            make --quiet -j"$(nproc)" 2>/dev/null
        warn "Installing (sudo)..."
            sudo make install --quiet 2>/dev/null
        sudo ldconfig
        ok "bcm2835-${BCM_VER} installed from source"
    else
        warn "Source download failed, trying apt..."
        sudo apt-get install -y -qq libbcm2835-dev 2>/dev/null && \
            ok "bcm2835 installed via apt" || \
            fail "Could not install bcm2835. Try manual install from $BCM_URL"
    fi

    cd "$REPO_DIR"
    rm -rf "$TMPDIR"
fi

# ---- verify ----
echo ""
echo "============================================"
echo " Verification"
echo "============================================"

if g++ --version >/dev/null 2>&1; then
    ok "g++: $(g++ --version | head -1)"
else
    fail "g++ not found"
fi

if ldconfig -p | grep -q libbcm2835; then
    ok "libbcm2835 found in ldconfig"
elif [ -f /usr/local/lib/libbcm2835.a ]; then
    ok "libbcm2835.a found in /usr/local/lib"
else
    fail "libbcm2835 not found"
fi

echo ""
warn "Running build test..."
if make -C gpios clean >/dev/null 2>&1 && make -C gpios -j"$(nproc)" >/dev/null 2>&1; then
    ok "Build successful: gpios/bin/gpio_generator"
else
    fail "Build failed"
fi

echo ""
echo "============================================"
echo " Done."
echo " Run: cd $REPO_DIR && sudo ./gpios/bin/gpio_generator"
echo "============================================"
