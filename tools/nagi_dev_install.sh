#!/bin/zsh
# Builds Nagi, installs it over the one already installed, and restarts its processes.
# For updating during development: no logout, no adding the input source again.
# The first install, and changes to the input modes or menu bar icons (Info.plist),
# still need a logout.
# Usage: zsh tools/nagi_dev_install.sh [--no-build]
set -eu

src="${0:A:h}/../src"
pkg="$src/bazel-bin/mac/Nagi.pkg"

if [[ "${1:-}" != "--no-build" ]]; then
  # A Bazel server left over from another shell fails with "getconf failed".
  (cd "$src" && bazelisk shutdown >/dev/null 2>&1; bazelisk build package --config release_build)
fi

echo "Installing $pkg (asks for your password)"
sudo installer -pkg "$pkg" -target /

# macOS starts the new ones on the next key. llama-server goes with NagiConverter.
killall Nagi NagiConverter NagiRenderer 2>/dev/null || true
echo "Done. If typing does not come back, switch the input source to ABC and back."
