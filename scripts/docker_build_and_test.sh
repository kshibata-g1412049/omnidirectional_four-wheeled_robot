#!/usr/bin/env bash
# ===========================================================================
#  docker_build_and_test.sh - build the image and run the headless smoke test
#
#  - Builds the Docker image (passing through an optional HTTP(S) proxy and CA
#    bundle so it works on restricted build networks).
#  - Runs scripts/smoke_test.sh inside the container.
#
#  Usage:
#    bash scripts/docker_build_and_test.sh [image_tag]
#
#  Honors these environment variables when set:
#    HTTPS_PROXY / HTTP_PROXY / NO_PROXY   - forwarded as docker build args
#    EXTRA_CA_BUNDLE                        - path to a CA cert to trust in build
# ===========================================================================
set -euo pipefail

IMAGE="${1:-omni4wd:jazzy}"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

# --- proxy / CA wiring (optional) ------------------------------------------
BUILD_ARGS=()
NET_ARGS=()
if [ -n "${HTTPS_PROXY:-${https_proxy:-}}" ]; then
  PROXY="${HTTPS_PROXY:-$https_proxy}"
  echo "[build] using proxy: $PROXY (build with --network=host)"
  BUILD_ARGS+=(--build-arg "http_proxy=$PROXY" --build-arg "https_proxy=$PROXY"
               --build-arg "no_proxy=${NO_PROXY:-${no_proxy:-localhost,127.0.0.1}}")
  NET_ARGS+=(--network=host)
fi

# Stage a CA bundle into the build context if one was provided.
CA_SRC="${EXTRA_CA_BUNDLE:-/root/.ccr/ca-bundle.crt}"
CLEAN_CA=0
if [ -f "$CA_SRC" ] && [ ! -f "$REPO_DIR/ca-bundle.crt" ]; then
  echo "[build] staging CA bundle from $CA_SRC"
  cp "$CA_SRC" "$REPO_DIR/ca-bundle.crt"
  CLEAN_CA=1
fi
cleanup_ca() { [ "$CLEAN_CA" -eq 1 ] && rm -f "$REPO_DIR/ca-bundle.crt" || true; }
trap cleanup_ca EXIT

# --- build -----------------------------------------------------------------
echo "[build] docker build -t $IMAGE ..."
docker build "${NET_ARGS[@]}" "${BUILD_ARGS[@]}" -t "$IMAGE" "$REPO_DIR"

# --- run smoke test --------------------------------------------------------
echo "[test] running smoke_test.sh inside the container ..."
RC=0
docker run --rm "${NET_ARGS[@]}" "$IMAGE" smoke_test.sh || RC=$?

if [ $RC -eq 0 ]; then
  echo "[test] RESULT: PASS"
else
  echo "[test] RESULT: FAIL (rc=$RC)"
fi
exit $RC
