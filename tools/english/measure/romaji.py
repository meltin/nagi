"""Romaji as Mozc reads it, for measuring the automatic input mode.

to_kana() follows the composer's table (src/data/preedit/romanji-hiragana.tsv):
the longest entry that matches is taken, and its "pending" part is typed again.
to_romaji() goes the other way, to turn test readings into the keys a user
would type. Both exist only for measurement; the IME itself uses the composer.
"""
import os

_TABLE_PATH = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src',
                           'data', 'preedit', 'romanji-hiragana.tsv')


def _load_table():
  table = {}
  with open(_TABLE_PATH, encoding='utf-8') as lines:
    for line in lines:
      fields = line.rstrip('\n').split('\t')
      if len(fields) >= 2 and fields[0]:
        table[fields[0]] = (fields[1], fields[2] if len(fields) > 2 else '')
  return table


TABLE = _load_table()
_MAX_KEY = max(len(key) for key in TABLE)


def to_kana(keys, end_of_input=True):
  """Returns what |keys| reads as. Letters that do not read stay as they are.

  A trailing "n" becomes "ん" when |end_of_input|, as it does when the
  composition is converted.
  """
  out = []
  rest = keys
  while rest:
    for size in range(min(_MAX_KEY, len(rest)), 0, -1):
      entry = TABLE.get(rest[:size])
      if entry is not None:
        out.append(entry[0])
        rest = entry[1] + rest[size:]
        break
    else:
      out.append(rest[0])
      rest = rest[1:]
  kana = ''.join(out)
  if end_of_input and kana.endswith('n'):
    kana = kana[:-1] + 'ん'
  return kana


def reads_as_kana(keys, end_of_input=True):
  """True if no key of |keys| is left unread."""
  return not any(c.isascii() and c.isalpha() for c in to_kana(keys, end_of_input))


def _inverse(preferred):
  inverse = {}
  for key, (output, pending) in TABLE.items():
    if pending or not key.isascii() or not key.isalpha() or not output:
      continue
    current = inverse.get(output)
    # Rows such as "ca" for "か" exist in the table, but nobody types Japanese that way.
    odd = key[0] in 'cqxl' and not key.startswith('ch')
    rank = (0 if key in preferred else 1, odd, len(key), key)
    if current is None or rank < current[0]:
      inverse[output] = (rank, key)
  return {kana: key for kana, (_, key) in inverse.items()}


# Two common ways to type the same kana.
_STYLES = {
    'hepburn': _inverse({'shi', 'chi', 'tsu', 'fu', 'ji', 'sha', 'shu', 'sho', 'cha', 'chu',
                         'cho', 'ja', 'ju', 'jo'}),
    'kunrei': _inverse({'si', 'ti', 'tu', 'hu', 'zi', 'sya', 'syu', 'syo', 'tya', 'tyu', 'tyo',
                        'zya', 'zyu', 'zyo'}),
}
_MARKS = {'ー': '-', '、': ',', '。': '.', '？': '?', '！': '!', '・': '/', '「': '[', '」': ']'}


def to_romaji(kana, style='hepburn'):
  """Returns keys that type |kana|, or None if some character cannot be typed."""
  inverse = _STYLES[style]
  out = []
  i = 0
  while i < len(kana):
    c = kana[i]
    if c == 'っ':
      following = to_romaji(kana[i + 1:i + 3], style) or to_romaji(kana[i + 1:i + 2], style)
      if not following or following[0] in 'aiueon':
        return None
      out.append(following[0])
      i += 1
      continue
    if c == 'ん':
      following = to_romaji(kana[i + 1:i + 3], style) or to_romaji(kana[i + 1:i + 2], style) or ''
      out.append('nn' if following[:1] in ('a', 'i', 'u', 'e', 'o', 'y', 'n') or not following else 'n')
      i += 1
      continue
    if c in _MARKS:
      out.append(_MARKS[c])
      i += 1
      continue
    for size in (2, 1):
      key = inverse.get(kana[i:i + size])
      if key is not None and len(kana[i:i + size]) == size:
        out.append(key)
        i += size
        break
    else:
      return None
  return ''.join(out)


if __name__ == '__main__':
  import sys
  for word in sys.argv[1:]:
    kana = to_kana(word)
    print(word, kana, to_romaji(kana), to_romaji(kana, 'kunrei'))
