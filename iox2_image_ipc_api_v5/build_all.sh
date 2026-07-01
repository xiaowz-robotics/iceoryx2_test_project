#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IOX2_INSTALL="${IOX2_INSTALL:-$HOME/IPC/iceoryx2/build/install}"

printf '\n========== Build iox2_image_ipc_lib =========='"\n"
cd "$ROOT_DIR/iox2_image_ipc_lib"
rm -rf build install
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install -DIOX2_INSTALL="$IOX2_INSTALL"
make -j"$(nproc)"
make install

printf '\n========== Build sender_app =========='"\n"
cd "$ROOT_DIR/sender_app"
rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DIPC_LIB_INSTALL="$ROOT_DIR/iox2_image_ipc_lib/install"
make -j"$(nproc)"

printf '\n========== Build receiver_app =========='"\n"
cd "$ROOT_DIR/receiver_app"
rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DIPC_LIB_INSTALL="$ROOT_DIR/iox2_image_ipc_lib/install"
make -j"$(nproc)"

cat <<MSG

Build complete.

Run receiver first:
  cd $ROOT_DIR/receiver_app/build
  ./receiver_app

Run sender in another terminal:
  cd $ROOT_DIR/sender_app/build
  ./sender_app
MSG
