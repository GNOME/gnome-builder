#!/usr/bin/env sh
#
# Build (and optionally install) GNOME Builder from this checkout.
#
# Usage:
#   ./build.sh              configure + build only
#   ./build.sh --deps       also install build dependencies (Fedora, needs sudo)
#   ./build.sh --install    build then install to /usr/local (needs sudo)
#   ./build.sh --deps --install
#
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$SCRIPT_DIR/_build"
PREFIX="/usr/local"

DO_DEPS=0
DO_INSTALL=0

for arg in "$@"; do
  case "$arg" in
    --deps)    DO_DEPS=1 ;;
    --install) DO_INSTALL=1 ;;
    -h|--help)
      sed -n '3,9p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      exit 2
      ;;
  esac
done

cd "$SCRIPT_DIR"

# 1. Install build dependencies. Fedora/RHEL only; skip on other distros.
if [ "$DO_DEPS" -eq 1 ]; then
  echo "==> Installing build dependencies (dnf builddep gnome-builder)"
  sudo dnf builddep -y gnome-builder
fi

# 2. Configure the Meson build directory. --wipe makes this idempotent and
#    safe to re-run after changing sources or options.
echo "==> Configuring Meson build in $BUILD_DIR"
meson setup --wipe "$BUILD_DIR" \
  --prefix="$PREFIX" \
  -Dhelp=false \
  -Dnetwork_tests=false \
  -Dplugin_deviced=false \
  -Dtracing=false \
  -Dtcmalloc=false

# 3. Compile.
echo "==> Building"
ninja -C "$BUILD_DIR"

# 4. Optionally install. This also recompiles the GSettings schemas and
#    updates the icon/desktop caches via the project's post-install scripts.
if [ "$DO_INSTALL" -eq 1 ]; then
  echo "==> Installing to $PREFIX"
  sudo ninja -C "$BUILD_DIR" install
  echo "==> Done. Restart any running Builder instance to use the new build."
fi
