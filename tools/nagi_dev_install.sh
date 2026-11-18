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
  # The English word list of the automatic input mode is not committed. Without
  # it the build works and the mode finds nothing, so make it once per Mac.
  if [[ ! -s "$src/data/english_nagi/generated/english_words.tsv" ]]; then
    echo "Making the English word list (once, downloads about 30 MB)"
    zsh "${0:A:h}/english/prepare_english_words.sh"
  fi
  # A Bazel server left over from another shell fails with "getconf failed".
  (cd "$src" && bazelisk shutdown >/dev/null 2>&1; bazelisk build package --config release_build)
fi

echo "Installing $pkg (asks for your password)"
sudo installer -pkg "$pkg" -target /

# launchd remembers the signature of the binaries it loaded, and refuses to start
# rebuilt ones ("spawn failed", exit 78) until the jobs are loaded again. Without
# this the converter stays down: no conversion, and the settings cannot be saved.
for job in Converter Renderer; do
  label="org.nagi.inputmethod.Japanese.$job"
  launchctl bootout "gui/$UID/$label" 2>/dev/null || true
  # bootout returns before a running job is gone, and bootstrap fails until then
  # ("Bootstrap failed: 5: Input/output error").
  for try in {1..20}; do
    launchctl bootstrap "gui/$UID" "/Library/LaunchAgents/$label.plist" 2>/dev/null && break
    sleep 0.5
  done
  launchctl print "gui/$UID/$label" >/dev/null || { echo "Could not reload $label. Log out and in."; exit 1; }
done

# macOS starts a new one on the next key. llama-server goes with NagiConverter.
killall Nagi 2>/dev/null || true
echo "Done. If typing does not come back, switch the input source to ABC and back."
