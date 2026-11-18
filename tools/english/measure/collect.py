#!/usr/bin/env python3
"""Collects, for every English-word candidate in a set of test inputs, the numbers a
rule could use to decide whether it is English. Writes one TSV row per candidate.

  python3 tools/english/measure/collect.py OUT.tsv

Inputs (built here, nothing is hand-labelled per candidate):
  japanese_sentences  readings of Mozc's own evaluation sets; every candidate is label 0
  japanese_words      readings of dictionary words, most frequent first; label 0
  mixed               an English word put into a Japanese carrier phrase; that word is
                      label 1, every other candidate in the phrase is label 0

Needs the cost tool: (cd src && bazelisk build //converter:reading_cost_main --config release_build)
"""
import collections
import glob
import os
import random
import subprocess
import sys

import candidates
import romaji

ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..')
COST_TOOL = os.path.join(ROOT, 'src', 'bazel-bin', 'converter', 'reading_cost_main')
STYLES = ('hepburn', 'kunrei')

# Carrier phrases, as readings. "{}" is where the English word goes.
CARRIERS = [
    '{}をつかう', '{}でやる', 'きょうは{}をする', '{}について', '{}がうごかない',
    'あたらしい{}をためした', '{}ってなに', '{}にする', '{}のせってい', 'この{}はいい',
    '{}した', '{}する', '{}してください', '{}をかくにんする', 'あしたの{}をついか',
    '{}でけんさく', 'その{}をみせて', '{}がひつよう', '{}とおなじ', '{}をなおした',
    'さっきの{}のけん', '{}になった', '{}はどこ', 'まず{}から', '{}もおねがい',
]


def japanese_sentences():
  keys = set()
  for path in ('src/data/dictionary_oss/evaluation.tsv',
               'src/data/test/quality_regression_test/oss.tsv',
               'src/data/test/quality_regression_test/regression.tsv'):
    with open(os.path.join(ROOT, path), encoding='utf-8') as lines:
      for line in lines:
        fields = line.split('\t')
        if len(fields) > 1 and not line.startswith('#'):
          keys.add(fields[1].strip())
  return sorted(keys)


def japanese_words(limit):
  best = {}
  for path in sorted(glob.glob(os.path.join(ROOT, 'src/data/dictionary_oss/dictionary0*.txt'))):
    with open(path, encoding='utf-8') as lines:
      for line in lines:
        fields = line.split('\t')
        if len(fields) >= 5:
          cost = int(fields[3])
          if cost < best.get(fields[0], 1 << 30):
            best[fields[0]] = cost
  return [key for key, _ in sorted(best.items(), key=lambda item: item[1])[:limit]]


def english_sample(words, rng):
  """Words to put into the carriers, from every part of the list."""
  groups = collections.defaultdict(list)
  for word, (size, sources) in words.items():
    if len(word) >= 3:
      groups[(size, sources)].append(word)
  sample = []
  for key in sorted(groups):
    members = sorted(groups[key])
    rng.shuffle(members)
    sample.extend(members[:60])
  return sample


def costs_of(readings):
  readings = sorted(set(readings))
  out = subprocess.run([COST_TOOL], input='\n'.join(readings) + '\n', capture_output=True,
                       text=True, check=True).stdout.splitlines()
  result = {}
  for line in out:
    cost, prefix, reading = line.split('\t')
    result[reading] = (int(cost), prefix == 'prefix')
  return result


def main():
  rng = random.Random(20261118)
  words = candidates.load_words()
  rows = []  # (set, keys, begin, end, label)

  def add(name, keys, english=None):
    for begin, end, word in candidates.find(keys, words):
      rows.append((name, keys, begin, end, int((begin, end) == english)))

  for reading in japanese_sentences():
    for style in STYLES:
      keys = romaji.to_romaji(reading, style)
      if keys:
        add('japanese_sentences', keys)
  for reading in japanese_words(int(os.environ.get('NAGI_WORD_LIMIT', '60000'))):
    keys = romaji.to_romaji(reading)
    if keys:
      add('japanese_words', keys)
  for word in english_sample(words, rng):
    for carrier in rng.sample(CARRIERS, 6):
      before, after = carrier.split('{}')
      keys_before = romaji.to_romaji(before) if before else ''
      keys = keys_before + word + romaji.to_romaji(after)
      add('mixed', keys, (len(keys_before), len(keys_before) + len(word)))

  readings = []
  for _, keys, begin, end, _ in rows:
    word = keys[begin:end]
    readings += [romaji.to_kana(keys),
                 romaji.to_kana(keys[:begin]) + word + romaji.to_kana(keys[end:]),
                 romaji.to_kana(word)]
  costs = costs_of(readings)

  with open(sys.argv[1], 'w', encoding='utf-8') as out:
    out.write('set\tkeys\tbegin\tend\tword\tlabel\tscowl\tsources\treads\tcost_kana\t'
              'cost_english\tcost_word\tword_is_prefix\n')
    for name, keys, begin, end, label in rows:
      word = keys[begin:end]
      size, sources = words[word]
      out.write('\t'.join(map(str, [
          name, keys, begin, end, word, label, size, sources or '-',
          int(romaji.reads_as_kana(word)),
          costs[romaji.to_kana(keys)][0],
          costs[romaji.to_kana(keys[:begin]) + word + romaji.to_kana(keys[end:])][0],
          costs[romaji.to_kana(word)][0], int(costs[romaji.to_kana(word)][1]),
      ])) + '\n')
  print(len(rows), 'candidates')


if __name__ == '__main__':
  main()
