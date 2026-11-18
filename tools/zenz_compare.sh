#!/bin/zsh
# Types each line of a file of romaji readings twice, with and without Zenz, and prints
# what Enter would commit. For judging a change to the rules that accept a Zenz answer:
# count the lines Zenz made better and the lines it made worse, before and after.
#   zsh tools/zenz_compare.sh tools/zenz_cases/homophones.txt
# Every reading gets a fresh profile, so one line cannot teach the next. The tool sends
# no preceding text, unlike a real application.
# Build first: (cd src && bazelisk build //session:nagi_typing_main --config release_build)
set -eu
bin="${0:A:h}/../src/bazel-bin/session/nagi_typing_main"
commit() {  # zenz flag, reading
  "$bin" --zenz=$1 --profile="$(mktemp -d)" "$2" 2>/dev/null |
    sed -n 's/^  commit  \(.*\)  ([0-9]* callbacks)$/\1/p'
}
while IFS= read -r reading; do
  [[ -z "$reading" || "$reading" == \#* ]] && continue
  engine="$(commit false "$reading")"
  zenz="$(commit true "$reading")"
  if [[ "$engine" == "$zenz" ]]; then
    print -r -- "same    $engine"
  else
    print -r -- "CHANGED $engine -> $zenz"
  fi
done < "$1"
