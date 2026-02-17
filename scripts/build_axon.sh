#!/bin/bash
set -e

# This script is intended to be run ON THE AXON DEVICE (or a compatible ARM64 container).
# It builds the JetKVM application using system libraries (glibc) instead of the embedded toolchain.

SCRIPT_PATH=$(realpath "$(dirname $(realpath "${BASH_SOURCE[0]}"))")
PROJECT_ROOT=$(realpath "${SCRIPT_PATH}/..")
CGO_PATH=$(realpath "${PROJECT_ROOT}/internal/native/cgo")
BUILD_DIR=${CGO_PATH}/build_axon

# Ensure we are on the project root for go build
cd ${PROJECT_ROOT}

echo "═══════════════════════════════════════════════════════"
echo "  JetKVM Axon Build Script"
echo "═══════════════════════════════════════════════════════"

if [ -z "$(command -v go)" ]; then
    echo "Error: 'go' not found. Please install Go 1.22+."
    exit 1
fi
if [ -z "$(command -v cmake)" ]; then
    echo "Error: 'cmake' not found. Please install cmake."
    exit 1
fi
if [ -z "$(command -v gcc)" ]; then
    echo "Error: 'gcc' not found. Please install build-essential."
    exit 1
fi

echo "▶ Generating UI index..."
cd ${CGO_PATH}
./ui_index.gen.sh
cd ${PROJECT_ROOT}

echo "▶ Building Native Library (jknative)..."
# Clean previous build
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Run CMake with the Axon-specific list file
# We use -f to specify the file? standard cmake doesn't support -f easily directly, 
# typically expects CMakeLists.txt in the source dir.
# Workaround: copy CMakeLists_axon.txt to CMakeLists.txt temporarily? 
# OR use -C pre-load? No.
# Best way: Symlink if we can, or just tell the user we swaped it.
# Let's just assume we can rename it.
# Trap to restore original CMakeLists.txt on exit
cleanup() {
    if [ -f "${CGO_PATH}/CMakeLists.txt.bak" ]; then
        mv "${CGO_PATH}/CMakeLists.txt.bak" "${CGO_PATH}/CMakeLists.txt"
    fi
}
trap cleanup EXIT

# Force remove lv_conf.h if it exists to ensure we re-copy and patch it correctly
if [ -f "${CGO_PATH}/lv_conf.h" ]; then
    rm "${CGO_PATH}/lv_conf.h"
fi

# Ensure lv_conf.h exists (required by LVGL os_desktop build)
if [ ! -f "${CGO_PATH}/lv_conf.h" ]; then
    if [ -f "${CGO_PATH}/include/lvgl/lv_conf.h" ]; then
        echo "▶ Copying lv_conf.h from include/lvgl/..."
        cp "${CGO_PATH}/include/lvgl/lv_conf.h" "${CGO_PATH}/lv_conf.h"
        
        # Patch lv_conf.h to remove C struct declarations that break assembly files
        # (lv_blend_helium.S includes this and fails on 'struct')
        echo "▶ Patching lv_conf.h for assembly compatibility..."
        sed -i '/struct _silence_gcc_warning;/d' "${CGO_PATH}/lv_conf.h"
    else
        echo "▶ Creating minimal lv_conf.h..."
        cat << EOF > "${CGO_PATH}/lv_conf.h"
#ifndef LV_CONF_H
#define LV_CONF_H
/* Enable Kconfig usage */
#define LV_CONF_SKIP
#define LV_USE_KCONFIG
#endif
EOF
    fi
fi

if [ -f "${CGO_PATH}/CMakeLists.txt" ]; then
    cp "${CGO_PATH}/CMakeLists.txt" "${CGO_PATH}/CMakeLists.txt.bak"
fi
cp "${CGO_PATH}/CMakeLists_axon.txt" "${CGO_PATH}/CMakeLists.txt"

cmake -B "${BUILD_DIR}" -S "${CGO_PATH}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install"

# Restore original CMakeLists immediately to be safe
if [ -f "${CGO_PATH}/CMakeLists.txt.bak" ]; then
    mv "${CGO_PATH}/CMakeLists.txt.bak" "${CGO_PATH}/CMakeLists.txt"
else
    rm "${CGO_PATH}/CMakeLists.txt" # It was new? unlikely.
fi

echo "▶ Compiling jknative (single threaded to save RAM)..."
cmake --build "${BUILD_DIR}" --target install -- -j1

# Copy headers and libs to where CGO expects them
echo "▶ Installing libraries for CGO..."
cp -r "${BUILD_DIR}/install/include" "${CGO_PATH}/"
cp -r "${BUILD_DIR}/install/lib" "${CGO_PATH}/"

# Build Go App
echo "▶ Building JetKVM Go Application..."
export CGO_ENABLED=1
export CGO_CFLAGS="-I${CGO_PATH}/include"
export CGO_LDFLAGS="-L${CGO_PATH}/lib -ljknative -llvgl -lrockchip_mpp -lrga -lpthread -lm" 
# Note: we link directly here. If rockit is needed and found by cmake, it's inside jknative dependencies? 
echo "▶ Tip: If you see undefined macro errors (like RK_LOGE), try 'rm -rf ${BUILD_DIR}' to force recompile."
# rockchip_mpp and rga are dynamic usually? If static, we need them here.
# If dynamic, we link them.

# Check if we need -lrockit
if [ -f "/usr/lib/librockit.so" ] || [ -f "/usr/lib/aarch64-linux-gnu/librockit.so" ]; then
    export CGO_LDFLAGS="${CGO_LDFLAGS} -lrockit"
fi

go build -v -tags "netgo,timetzdata,nomsgpack" -o bin/jetkvm_app cmd/main.go

echo "═══════════════════════════════════════════════════════"
echo "  Build Complete!"
echo "  Binary: bin/jetkvm_app"
echo "═══════════════════════════════════════════════════════"
