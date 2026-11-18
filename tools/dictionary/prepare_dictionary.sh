#!/bin/zsh
# Generates the extra system dictionaries Mozkey ships ("daily" profile) and puts them
# where the Nagi build picks them up: src/data/dictionary_nagi/generated/.
#
# Nothing here is Nagi's own dictionary work. The sources, their pinned revisions and
# hashes, the filters and the checks all come from Mozkey at the commit below; this
# script only runs Mozkey's steps on a Mac, against Nagi's base dictionary.
# (Mozkey drives the same steps with PowerShell: tools/dictionary/prepare_daily_dictionary.ps1)
#
# Usage: zsh tools/dictionary/prepare_dictionary.sh
# Needs git, bzip2, curl and uv. Downloads about 50 MB. The generated files are not
# committed. Rebuild Nagi afterwards. To go back to the plain Mozc dictionary, delete
# src/data/dictionary_nagi/generated/*.txt and rebuild.
set -euo pipefail

# Mozkey v1.1.0. To follow a newer Mozkey, change this and read its lock file
# (tools/dictionary/locks/daily_sources.json) and DICTIONARY_NOTICES.md first.
MOZKEY_REVISION=7f8af6b852907aab93d1551530407a434c6f044c
MOZKEY_URL=https://github.com/koyasi777/mozkey.git
MERGE_UT_URL=https://github.com/utuhiro78/merge-ut-dictionaries.git
# Blob of src/merge/merge_dictionaries.py that Mozkey's patch was written for.
MERGE_SCRIPT_BLOB=778e4620117aa0eda85f7572a40413e2783cef9a

nagi="${0:A:h:h:h}"
out="$nagi/src/data/dictionary_nagi/generated"
cache="$HOME/Library/Caches/nagi-dictionary"
mozkey="$cache/mozkey"
# Mozkey's tools find their inputs relative to their own location, so lay out a
# scratch tree the way Mozkey's repository is laid out.
work="$cache/work"
tools="$work/tools/dictionary"
generated="$work/src/data/dictionary_koyasi/generated"
lock="$tools/locks/daily_sources.json"

step() { print -P "%F{cyan}== $1%f"; }
fetch_revision() {  # directory, url, revision
  git init -q "$1"
  git -C "$1" remote add origin "$2"
  git -C "$1" -c core.autocrlf=false fetch -q --depth 1 origin "$3"
  git -C "$1" checkout -q --detach FETCH_HEAD
  [[ "$(git -C "$1" rev-parse HEAD)" == "$3" ]]
}
lock_value() { python -c 'import json,sys; d=json.load(open(sys.argv[1])); [d:=d[k] for k in sys.argv[2].split(".")]; print(d)' "$lock" "$1"; }
download_locked() {  # lock section, destination
  local url="https://raw.githubusercontent.com/$(lock_value $1.repository)/$(lock_value $1.revision)/$(lock_value $1.path)"
  curl -fsSL "$url" -o "$2.part"
  [[ "$(shasum -a 256 "$2.part" | cut -d' ' -f1 | tr a-f A-F)" == "$(lock_value $1.sha256 | tr a-f A-F)" ]] ||
    { echo "SHA-256 of $url differs from Mozkey's lock"; exit 1; }
  mv "$2.part" "$2"
}

step "Mozkey's dictionary tools at $MOZKEY_REVISION"
mkdir -p "$cache"
if [[ "$(git -C "$mozkey" rev-parse HEAD 2>/dev/null)" != "$MOZKEY_REVISION" ]]; then
  rm -rf "$mozkey"
  fetch_revision "$mozkey" "$MOZKEY_URL" "$MOZKEY_REVISION"
fi

# The tools call "python". Give them a recent one without touching the system.
uv venv -q --python 3.12 --allow-existing "$cache/venv"
export PATH="$cache/venv/bin:$PATH"

rm -rf "$work"
mkdir -p "$work/tools" "$work/src/data" "$generated/nico_pixiv" "$generated/personal_names" "$generated/profiled"
cp -R "$mozkey/tools/dictionary" "$tools"
cp -R "$nagi/src/data/dictionary_oss" "$work/src/data/dictionary_oss"
cp -R "$mozkey/src/data/dictionary_koyasi/evaluation" "$work/src/data/dictionary_koyasi/evaluation"
python "$tools/pin_dictionary_inputs.py" --lock "$lock" validate

step "Wikipedia index (pinned snapshot, used only to weigh the entries)"
# Kept outside the scratch tree so that it is downloaded once.
mkdir -p "$cache/dist" && ln -s "$cache/dist" "$work/dist"
wiki="$(python "$tools/pin_dictionary_inputs.py" --lock "$lock" stage-wiki --repo-root "$work" | tail -1)"
[[ -f "$wiki" ]]

step "merge-ut-dictionaries: place names and SudachiDict"
merge="$work/merge-ut-dictionaries"
fetch_revision "$merge" "$MERGE_UT_URL" "$(lock_value merge_ut.revision)"
[[ "$(git -C "$merge" hash-object -- src/merge/merge_dictionaries.py)" == "$MERGE_SCRIPT_BLOB" ]] ||
  { echo "merge_dictionaries.py is not the one Mozkey's patch was written for"; exit 1; }
git -C "$merge" apply --unidiff-zero "$tools/patches/merge_ut_mozkey_local.patch"
python "$tools/tests/test_merge_ut_patch.py" --script "$merge/src/merge/merge_dictionaries.py"
# The "daily" profile: only these two sources.
python - "$merge/src/merge/make.sh" <<'PY'
import re, sys
path = sys.argv[1]
text = open(path, encoding='utf-8').read()
enabled = {'place_names', 'sudachidict'}
for name in ('alt_cannadic', 'edict2', 'jawiki', 'neologd', 'personal_names',
             'place_names', 'skk_jisyo', 'sudachidict', 'generate_latest'):
    pattern = r'(?m)^#?' + re.escape(f'{name}="true"') + r'$'
    assert len(re.findall(pattern, text)) == 1, f'make.sh layout changed: {name}'
    text = re.sub(pattern, ('' if name in enabled else '#') + f'{name}="true"', text)
assert text.startswith('#!')
head, rest = text.split('\n', 1)
open(path, 'w', encoding='utf-8').write(head + '\nset -eo pipefail\n' + rest)
PY
python "$tools/pin_dictionary_inputs.py" --lock "$lock" patch-staged-merge \
  --make "$merge/src/merge/make.sh" --merge-script "$merge/src/merge/merge_dictionaries.py"
(cd "$merge/src/merge" &&
  MOZKEY_JAWIKI_INDEX_BZ2="$wiki" MOZKEY_DICTIONARY_OSS_DIR="$work/src/data/dictionary_oss" \
    bash --noprofile --norc ./make.sh)
[[ -s "$merge/src/merge/mozcdic-ut.txt" ]]
# Mozkey calls the unfiltered merge "safe"; the daily filter reads it under that name.
cp "$merge/src/merge/mozcdic-ut.txt" "$generated/mozcdic-ut-safe.txt"

step "Nico/Pixiv and personal names (pinned files)"
download_locked nico_pixiv "$generated/nico_pixiv/dic-nico-intersection-pixiv-google.txt"
download_locked personal_names "$generated/personal_names/mozcdic-ut-personal-names.txt.bz2"
bzip2 -dk "$generated/personal_names/mozcdic-ut-personal-names.txt.bz2"

step "Mozkey's filters, checks and syntax guard"
python "$tools/profile_merge_ut.py" --profile daily
python "$tools/convert_nico_pixiv.py"
python "$tools/profile_personal_names.py"
python "$tools/check_merge_ut_profile.py" --profile daily
python "$tools/generate_syntax_guard_dictionary.py"

step "Install into $out"
# All four or none: a partial set would build without any sign of being incomplete.
files=(mozcdic-ut-daily.txt dic-nico-pixiv-delta.txt mozcdic-ut-personal-names-daily.txt koyasi-syntax-guard.txt)
for f in $files; do [[ -s "$generated/profiled/$f" ]] || { echo "missing: $f"; exit 1; }; done
mkdir -p "$out"
rm -f "$out"/*.txt(N)  # None yet on a fresh checkout.
for f in $files; do cp "$generated/profiled/$f" "$out/$f"; done
wc -l "$out"/*.txt
# The dictionary is compiled into the binaries through a generated include file, and
# Bazel on this setup does not notice when such files change. Drop the objects that
# embed it, or the next build keeps the old dictionary.
find -L "$nagi/src/bazel-out/" -path '*/_objs/*' -name '*.o' -path '*data_manager*' -delete 2>/dev/null || true
echo "Done. Rebuild Nagi: zsh tools/nagi_dev_install.sh"
