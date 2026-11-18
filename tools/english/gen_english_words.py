#!/usr/bin/env python3
"""Writes the English word list of the automatic input mode.

Output: one word per line, lower case, sorted:
  word <TAB> scowl_size <TAB> sources

scowl_size is the smallest SCOWL size the word appears in (35 = most common,
up to 70), or 0 when SCOWL does not have it. sources is a string of letters:
"c" the word is in a cspell-dicts dictionary, "d" it is an ASCII value of the
conversion dictionary. How these make a word more or less English is decided
by the detector, not here: this file only records where a word was seen.
"""
import argparse
import glob
import os
import re

MAX_SCOWL_SIZE = 70
# Abbreviations, prefixes, suffixes and the like are not words one types alone.
SKIPPED_SCOWL_TAGS = ('<abbr', '<pre', '<suf', '<x', '<wp')
WORD = re.compile(r'[A-Za-z]{2,24}')


def scowl_words(path):
  """Yields (size, word) for the entries of scowl-pre.txt."""
  with open(path, encoding='utf-8') as lines:
    for line in lines:
      # SIZE [REGION ...] ': ' [VARIANT-INFO ': '] LEMMA <POS> [': ' ENTRY, ...]
      parts = line.split('#')[0].rstrip().split(': ')
      head = parts[0].split()
      if not head or not head[0].isdigit():
        continue
      size = int(head[0])
      lemma_info = parts[-2] if len(parts) >= 3 else parts[-1]
      if size > MAX_SCOWL_SIZE or any(t in lemma_info for t in SKIPPED_SCOWL_TAGS):
        continue
      lemma = lemma_info.split(' <')[0].strip()
      entries = parts[-1].split(', ') if len(parts) >= 3 else []
      for word in [lemma] + entries:
        word = word.strip().rstrip('*+!~-@').strip()
        if WORD.fullmatch(word):
          yield size, word.lower()


def cspell_words(directories):
  for directory in directories:
    for path in sorted(glob.glob(os.path.join(directory, 'src', '*.txt'))):
      with open(path, encoding='utf-8') as lines:
        for line in lines:
          # "word", with cspell's flags: "!word" (forbidden), "+word", "word*".
          word = line.split('#')[0].strip()
          if word.startswith('!'):
            continue
          word = word.strip('+*~')
          if WORD.fullmatch(word):
            yield word.lower()


def dictionary_words(paths):
  for path in paths:
    with open(path, encoding='utf-8') as lines:
      for line in lines:
        fields = line.rstrip('\n').split('\t')
        if len(fields) >= 5 and WORD.fullmatch(fields[4]):
          yield fields[4].lower()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--scowl', required=True)
  parser.add_argument('--cspell', nargs='*', default=[])
  parser.add_argument('--dictionary', nargs='*', default=[])
  parser.add_argument('--output', required=True)
  args = parser.parse_args()

  sizes = {}
  for size, word in scowl_words(args.scowl):
    sizes[word] = min(size, sizes.get(word, size))
  cspell = set(cspell_words(args.cspell))
  dictionary = set(dictionary_words(args.dictionary))

  with open(args.output, 'w', encoding='utf-8') as out:
    for word in sorted(set(sizes) | cspell | dictionary):
      sources = ('c' if word in cspell else '') + ('d' if word in dictionary else '')
      out.write(f'{word}\t{sizes.get(word, 0)}\t{sources}\n')


if __name__ == '__main__':
  main()
