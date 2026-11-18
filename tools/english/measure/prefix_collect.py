#!/usr/bin/env python3
"""Like collect.py, but for every prefix of every input: what a rule would see after
each key. Writes one TSV row per (input, number of keys typed, candidate).

  python3 tools/english/measure/prefix_collect.py OUT.tsv

A row with an empty word is written for a prefix without candidates, so that every
prefix appears. Sizes can be set with NAGI_WORDS, NAGI_MIXED (inputs per set).
"""
import os
import random
import sys

import candidates
import collect
import romaji


def inputs(words, rng):
  """Yields (set, keys, english) with english = (begin, end) or None."""
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
  for index, (name, keys, english) in enumerate(inputs(words, rng)):
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
                     english))
      if not found:
        rows.append((name, index, keys, n, len(tail), 0, 0, '', kana, '', english))
  costs = collect.costs_of(readings)
  with open(sys.argv[1], 'w', encoding='utf-8') as out:
    out.write('set\tinput\tkeys\ttyped\ttail\tbegin\tend\tword\tenglish_begin\tenglish_end\t'
              'scowl\tsources\treads\tcost_kana\tcost_english\tcost_word\tword_is_prefix\n')
    for name, index, keys, n, tail, begin, end, word, kana, english_reading, english in rows:
      size, sources = words.get(word, (0, ''))
      word_kana = romaji.to_kana(word) if word else ''
      out.write('\t'.join(map(str, [
          name, index, keys, n, tail, begin, end, word or '-',
          english[0] if english else -1, english[1] if english else -1, size, sources or '-',
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
