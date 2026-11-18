#!/usr/bin/env python3
"""Like collect.py, but for every prefix of every input: what a rule would see after
each key. Writes one TSV row per (input, number of keys typed, candidate).

  python3 tools/english/measure/prefix_collect.py OUT.tsv

A row with an empty word is written for a prefix without candidates, so that every
prefix appears. Sizes can be set with NAGI_WORDS, NAGI_MIXED (inputs per set).
"""
import os
import random
import subprocess
import sys

import candidates
import collect
import romaji


JSUT_URL = ('https://raw.githubusercontent.com/sarulab-speech/jsut-label/master/'
            'text_kana/basic5000.yaml')
CACHE = os.path.expanduser('~/Library/Caches/nagi-english')


def spoken_sentences():
  """Readings of the 5,000 sentences of JSUT basic5000 (CC BY-SA 4.0; downloaded for the
  measurement, not kept in the repository). The corpus writes its kana as pronounced
  (the particle は as わ), which nobody types, so the readings are made from the text
  with the converter's reverse conversion instead."""
  path = os.path.join(CACHE, 'basic5000.yaml')
  if not os.path.exists(path):
    os.makedirs(CACHE, exist_ok=True)
    subprocess.run(['curl', '-fsSL', JSUT_URL, '-o', path], check=True)
  with open(path, encoding='utf-8') as lines:
    texts = [line.split(':', 1)[1].strip() for line in lines if 'text_level0:' in line]
  readings = subprocess.run([collect.COST_TOOL, '--reverse'], input='\n'.join(texts) + '\n',
                            capture_output=True, text=True, check=True).stdout.splitlines()
  # Keep the sentences that came back as kana only.
  kana = [''.join(c for c in reading if c not in '。、「」？！・') for reading in readings]
  return [reading for reading in kana
          if reading and all('ぁ' <= c <= 'ゖ' or c == 'ー' for c in reading)]


def held_out():
  """Inputs written by hand for Mozless, another IME by the same author. Never used
  to choose the rule. Yields (set, keys, display)."""
  root = os.environ.get('NAGI_HELD_OUT',
                        os.path.expanduser('~/dev/oss/mozless/src/data/test/english_runs'))
  if not os.path.isdir(root):
    return
  with open(os.path.join(root, 'mixed.tsv'), encoding='utf-8') as lines:
    for line in lines:
      fields = line.rstrip('\n').split('\t')
      if len(fields) == 2 and not line.startswith('#'):
        yield 'held_out_mixed', fields[0], fields[1]
  with open(os.path.join(root, 'japanese_sentences.txt'), encoding='utf-8') as lines:
    for line in lines:
      keys = line.strip()
      if keys and not keys.startswith('#'):
        yield 'held_out_japanese', keys, romaji.to_kana(keys)


def inputs(words, rng):
  """Yields (set, keys, display): the keys to type and what should be shown at the end."""
  for name, keys, english in _generated(words, rng):
    if english is None:
      yield name, keys, romaji.to_kana(keys)
    else:
      begin, end = english
      yield name, keys, romaji.to_kana(keys[:begin]) + keys[begin:end] + romaji.to_kana(keys[end:])
  yield from held_out()


def _generated(words, rng):
  for reading in collect.japanese_sentences():
    for style in collect.STYLES:
      keys = romaji.to_romaji(reading, style)
      if keys:
        yield 'japanese_sentences', keys, None
  japanese = collect.japanese_words(60000)
  step = max(1, len(japanese) // int(os.environ.get('NAGI_WORDS', '20000')))
  for reading in japanese[::step]:
    keys = romaji.to_romaji(reading)
    if keys:
      yield 'japanese_words', keys, None
  text = [keys for keys in map(romaji.to_romaji, spoken_sentences()) if keys]
  for keys in text:
    yield 'japanese_text', keys, None
  # Words that are common and known to spell checkers for code.
  common = sorted(word for word, (size, sources) in words.items()
                  if size == 35 and 'c' in sources and len(word) >= 3)
  common = sorted(rng.sample(common, min(len(common), 500)))
  for word in common:
    for carrier in rng.sample(collect.CARRIERS, 4):
      before, after = carrier.split('{}')
      keys_before = romaji.to_romaji(before) if before else ''
      yield ('mixed_common', keys_before + word + romaji.to_romaji(after),
             (len(keys_before), len(keys_before) + len(word)))
  # The same, between two real sentences: the composition is long enough for the
  # detector to stop looking at its beginning while the word is in it.
  for word in rng.sample(common, min(len(common), int(os.environ.get('NAGI_LONG', '1000')))):
    before, after = rng.choice(collect.CARRIERS).split('{}')
    head = rng.choice(text) + (romaji.to_romaji(before) if before else '')
    yield ('mixed_long', head + word + romaji.to_romaji(after) + rng.choice(text),
           (len(head), len(head) + len(word)))
  mixed = []
  for word in collect.english_sample(words, rng):
    for carrier in rng.sample(collect.CARRIERS, 6):
      before, after = carrier.split('{}')
      keys_before = romaji.to_romaji(before) if before else ''
      mixed.append(('mixed', keys_before + word + romaji.to_romaji(after),
                    (len(keys_before), len(keys_before) + len(word))))
  rng.shuffle(mixed)
  yield from mixed[:int(os.environ.get('NAGI_MIXED', '3000'))]


def main():
  rng = random.Random(20261118)
  words = candidates.load_words()
  prefixes = candidates.word_prefixes(words)
  rows = []
  progress = []  # (set, input, keys, typed, begin): an English word being typed
  readings = set()
  for index, (name, keys, display) in enumerate(inputs(words, rng)):
    for n in range(1, len(keys) + 1):
      prefix = keys[:n]
      if not prefix.isascii() or not prefix.replace('-', 'a').isalpha():
        continue  # Digits and marks are left to the session; only letters are measured.
      body, tail = candidates.split_tail(prefix)
      kana = romaji.to_kana(body)
      readings.add(kana)
      found = list(candidates.find(body, words))
      if not romaji.reads_as_kana(body):
        # It cannot be Japanese as it stands. An English word may be half typed.
        begin = candidates.in_progress(prefix, prefixes)
        if begin is not None and prefix[begin:] not in words:
          progress.append((name, index, keys, n, begin))
      for begin, end, word in found:
        english_reading = romaji.to_kana(body[:begin]) + word + romaji.to_kana(body[end:])
        readings.update((english_reading, romaji.to_kana(word)))
        rows.append((name, index, keys, n, len(tail), begin, end, word, kana, english_reading,
                     display))
      if not found:
        rows.append((name, index, keys, n, len(tail), 0, 0, '', kana, '', display))
  costs = collect.costs_of(readings)
  with open(sys.argv[1], 'w', encoding='utf-8') as out:
    out.write('set\tinput\tkeys\ttyped\ttail\tbegin\tend\tword\tdisplay\t'
              'scowl\tsources\treads\tcost_kana\tcost_english\tcost_word\tword_is_prefix\n')
    for name, index, keys, n, tail, begin, end, word, kana, english_reading, display in rows:
      size, sources = words.get(word, (0, ''))
      word_kana = romaji.to_kana(word) if word else ''
      out.write('\t'.join(map(str, [
          name, index, keys, n, tail, begin, end, word or '-',
          display, size, sources or '-',
          int(bool(word) and romaji.reads_as_kana(word)), costs[kana][0],
          costs[english_reading][0] if word else 0, costs[word_kana][0] if word else 0,
          int(costs[word_kana][1]) if word else 0,
      ])) + '\n')
  with open(sys.argv[1] + '.progress', 'w', encoding='utf-8') as out:
    out.write('set\tinput\tkeys\ttyped\tbegin\n')
    for row in progress:
      out.write('\t'.join(map(str, row)) + '\n')
  print(len(rows), 'rows,', len(progress), 'half-typed words')


if __name__ == '__main__':
  main()
