#!/bin/sh
# Builds the Zenz runtime for Nagi and puts it in the user's Nagi profile.
#
# Zenz models use a pre-tokenizer name that upstream llama.cpp does not know,
# so llama-server is built from source with a one-line patch. The patch and the
# pinned versions follow Mozkey (https://github.com/koyasi777/mozkey).
#
# Result:
#   ~/Library/Application Support/Nagi/zenz/llama-server
#   ~/Library/Application Support/Nagi/zenz/model.gguf
#
# Requires git, cmake and curl. Nothing outside the destination and the build
# cache is written.
set -eu

LLAMA_REPOSITORY="https://github.com/ggml-org/llama.cpp.git"
LLAMA_COMMIT="620d2225cc436ab0b001e3200e590e3b286ef9ab"
MODEL_URL="https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf/resolve/c67e03e07d215c869f591b274c1631170d3e11fe/ggml-model-Q5_K_M.gguf"
MODEL_SHA256="29c223d4c23327b80fd13ebb5ab2555057a46317997d5da391584ffbef0db673"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEST="${NAGI_ZENZ_DIR:-$HOME/Library/Application Support/Nagi/zenz}"
BUILD="${NAGI_ZENZ_BUILD_DIR:-$HOME/Library/Caches/nagi-zenz-build}"

mkdir -p "$DEST" "$BUILD"

if [ ! -x "$DEST/llama-server" ]; then
  if [ ! -d "$BUILD/llama.cpp/.git" ]; then
    git init -q "$BUILD/llama.cpp"
    git -C "$BUILD/llama.cpp" remote add origin "$LLAMA_REPOSITORY"
  fi
  git -C "$BUILD/llama.cpp" fetch -q --depth 1 origin "$LLAMA_COMMIT"
  git -C "$BUILD/llama.cpp" checkout -q -f FETCH_HEAD
  git -C "$BUILD/llama.cpp" apply "$SCRIPT_DIR/llama-vocab-gpt2-small-japanese-char.patch"
  cmake -S "$BUILD/llama.cpp" -B "$BUILD/out" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DGGML_ACCELERATE=ON \
    -DGGML_CCACHE=OFF \
    -DGGML_METAL=ON \
    -DGGML_METAL_EMBED_LIBRARY=ON \
    -DGGML_OPENMP=OFF \
    -DLLAMA_BUILD_COMMON=ON \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_MTMD=OFF \
    -DLLAMA_BUILD_SERVER=ON \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_TOOLS=ON \
    -DLLAMA_BUILD_UI=OFF \
    -DLLAMA_LLGUIDANCE=OFF \
    -DLLAMA_OPENSSL=OFF \
    -DLLAMA_USE_PREBUILT_UI=OFF
  cmake --build "$BUILD/out" --target llama-server -j "$(sysctl -n hw.ncpu)"
  cp "$BUILD/out/bin/llama-server" "$DEST/llama-server"
fi

if [ ! -f "$DEST/model.gguf" ]; then
  curl -fL --retry 3 -o "$DEST/model.gguf.part" "$MODEL_URL"
  actual="$(shasum -a 256 "$DEST/model.gguf.part" | cut -d ' ' -f 1)"
  if [ "$actual" != "$MODEL_SHA256" ]; then
    echo "model checksum mismatch: $actual" >&2
    rm -f "$DEST/model.gguf.part"
    exit 1
  fi
  mv "$DEST/model.gguf.part" "$DEST/model.gguf"
fi

echo "Zenz runtime is ready in: $DEST"
