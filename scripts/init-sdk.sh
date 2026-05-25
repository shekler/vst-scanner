#!/usr/bin/env bash
# Initialize and pin VST3 SDK submodule (v3.8.0) with nested submodules.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "[INFO] Updating vst3sdk submodule..."
git submodule update --init --recursive vst3sdk

cd vst3sdk
TAG="v3.8.0_build_66"
CURRENT="$(git describe --tags --exact-match 2>/dev/null || true)"
if [[ "$CURRENT" != "$TAG" ]]; then
    echo "[INFO] Checking out $TAG..."
    git fetch --tags origin
    git checkout "$TAG"
    git submodule update --init --recursive
fi

cd "$ROOT"
echo "[SUCCESS] VST3 SDK ready at $(git -C vst3sdk rev-parse --short HEAD)"
