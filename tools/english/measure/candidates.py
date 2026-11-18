"""Finds the places in typed keys where an English word could be read.

A candidate is a word of the English word list that appears in the keys so
that the keys before it and after it still read as kana. In Japanese text
every candidate is a chance to wrongly show English, so the measurement
counts them all.
"""
import os

import romaji

WORDS_PATH = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src', 'data',
                          'english_nagi', 'english_words.tsv')
MIN_LENGTH = 3
MAX_LENGTH = 24


def load_words(path=WORDS_PATH):
  """Returns {word: (scowl_size, sources)}."""
  words = {}
  with open(path, encoding='utf-8') as lines:
    for line in lines:
      word, size, sources = line.rstrip('\n').split('\t')
      # The conversion dictionary also has romanized Japanese names as ASCII values
      # ("ayaka", "kotoko"). Seen nowhere else and reading as kana from start to end,
      # they are Japanese.
      if size == '0' and sources == 'd' and romaji.reads_as_kana(word):
        continue
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


# What can still become a syllable: "k", "ky", "sh", and "tt" (a small tsu and a "t").
_UNFINISHED = ({key[:n] for key in romaji.TABLE for n in range(1, len(key))} |
               {key for key, (_, pending) in romaji.TABLE.items() if pending})


def split_tail(keys):
  """Splits |keys| being typed into the part that is finished and the consonants at
  the end that still wait for a vowel. Returns (body, tail)."""
  kana = romaji.to_kana(keys, end_of_input=False)
  waiting = len(kana) - len(kana.rstrip('abcdefghijklmnopqrstuvwxyz'))
  if not waiting:
    return keys, ''
  if kana[-waiting:] not in _UNFINISHED:
    return keys, ''  # A letter that does not read. Not a syllable in progress.
  for n in range(waiting, min(4, len(keys)) + 1):
    if romaji.reads_as_kana(keys[:len(keys) - n], end_of_input=False):
      return keys[:len(keys) - n], keys[len(keys) - n:]
  return keys, ''


def word_prefixes(words):
  """Every beginning, of 3 letters or more, of a word of the list."""
  return {word[:n] for word in words for n in range(MIN_LENGTH, len(word) + 1)}


def in_progress(keys, prefixes):
  """The longest end of |keys| that begins an English word, with kana before it.
  Returns its begin offset, or None."""
  for begin in range(max(0, len(keys) - MAX_LENGTH), len(keys) - MIN_LENGTH + 1):
    if keys[begin:] in prefixes and romaji.reads_as_kana(keys[:begin]):
      return begin
  return None
