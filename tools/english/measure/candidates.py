"""Finds the places in typed keys where an English word could be read.

A candidate is a word of the English word list that appears in the keys so
that the keys before it and after it still read as kana. In Japanese text
every candidate is a chance to wrongly show English, so the measurement
counts them all.
"""
import os

import romaji

WORDS_PATH = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src', 'data',
                          'english_nagi', 'generated', 'english_words.tsv')
MIN_LENGTH = 3
MAX_LENGTH = 24


def load_words(path=WORDS_PATH):
  """Returns {word: (scowl_size, sources)}."""
  words = {}
  with open(path, encoding='utf-8') as lines:
    for line in lines:
      word, size, sources = line.rstrip('\n').split('\t')
      words[word] = (int(size), sources)
  return words


def find(keys, words):
  """Yields (begin, end, word) for every candidate in |keys|."""
  for begin in range(len(keys)):
    if not romaji.reads_as_kana(keys[:begin]):
      continue
    for end in range(begin + MIN_LENGTH, min(len(keys), begin + MAX_LENGTH) + 1):
      word = keys[begin:end]
      if not word.isalpha():
        break
      if word in words and romaji.reads_as_kana(keys[end:]):
        yield begin, end, word
