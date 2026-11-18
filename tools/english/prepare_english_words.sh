#!/bin/zsh
# Rebuilds the English word list the automatic input mode uses to tell English words
# from romaji:
#   src/data/english_nagi/english_words.tsv
# The list is committed, so this is only for updating it.
#
# No list is written by hand. The words come from three places:
#   - SCOWL, ordinary English words with a "size" (smaller = more common),
#   - cspell-dicts, the terms, tools and companies spell checkers for code know,
#   - the Mozc dictionary, whose ASCII values are product and proper names.
# Their licenses are in src/data/english_nagi/LICENSES.md. Update it with the versions.
#
# Usage: zsh tools/english/prepare_english_words.sh
# Needs git, curl and python3. Downloads about 30 MB once.
set -euo pipefail

SCOWL_TAG=rel-2026.02.25
SCOWL_URL="https://raw.githubusercontent.com/en-wl/wordlist/$SCOWL_TAG/data/scowl-pre.txt"
SCOWL_LICENSE_URL="https://raw.githubusercontent.com/en-wl/wordlist/$SCOWL_TAG/Copyright"
CSPELL_URL=https://github.com/streetsidesoftware/cspell-dicts.git
CSPELL_REVISION=e0001cb0817aeab03edb29b9c7477bb54bb34acb
CSPELL_DICTIONARIES=(software-terms companies fullstack k8s docker git aws typescript python node)

nagi="${0:A:h:h:h}"
out="$nagi/src/data/english_nagi"
cache="$HOME/Library/Caches/nagi-english"
mkdir -p "$cache" "$out"

if [[ ! -s "$cache/scowl-pre-$SCOWL_TAG.txt" ]]; then
  curl -fsSL "$SCOWL_URL" -o "$cache/scowl-pre-$SCOWL_TAG.txt.part"
  mv "$cache/scowl-pre-$SCOWL_TAG.txt.part" "$cache/scowl-pre-$SCOWL_TAG.txt"
  curl -fsSL "$SCOWL_LICENSE_URL" -o "$cache/SCOWL-Copyright.txt"
fi

cspell="$cache/cspell-dicts"
if [[ "$(git -C "$cspell" rev-parse HEAD 2>/dev/null)" != "$CSPELL_REVISION" ]]; then
  rm -rf "$cspell"
  git init -q "$cspell"
  git -C "$cspell" remote add origin "$CSPELL_URL"
  git -C "$cspell" sparse-checkout set --no-cone LICENSE ${CSPELL_DICTIONARIES/#/dictionaries/}
  git -C "$cspell" fetch -q --depth 1 --filter=blob:none origin "$CSPELL_REVISION"
  git -C "$cspell" checkout -q --detach FETCH_HEAD
fi

python3 "${0:A:h}/gen_english_words.py" \
  --scowl "$cache/scowl-pre-$SCOWL_TAG.txt" \
  --cspell ${CSPELL_DICTIONARIES/#/$cspell/dictionaries/} \
  --dictionary "$nagi"/src/data/dictionary_oss/dictionary0*.txt \
  --output "$out/english_words.tsv.part"
mv "$out/english_words.tsv.part" "$out/english_words.tsv"
wc -l "$out/english_words.tsv"
