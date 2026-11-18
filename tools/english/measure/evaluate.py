#!/usr/bin/env python3
"""Applies a rule to the numbers of prefix_collect.py, key by key, and counts
- inputs whose final display is wrong (Japanese shown as English, or an English word
  missed), and
- inputs and keys where English was shown on the way although it should not have been.

  python3 tools/english/measure/evaluate.py PREFIX.tsv [--examples]

The rule here is a prototype for choosing thresholds. The IME's own detector is
measured with the same inputs once it exists.
"""
import collections
import csv
import os
import re
import sys

import romaji

INT_FIELDS = ('input', 'typed', 'tail', 'begin', 'end', 'scowl', 'reads', 'cost_kana',
              'cost_english', 'cost_word', 'word_is_prefix')
# Rule: a word that does not read as kana is English when showing it as English costs
# at most this much more than reading the keys as kana.
UNREADABLE_MARGIN = 5500
# A word that also reads as kana needs more: (minimum letters, minimum cost of the word
# alone as kana, margin). Any line that matches makes it English.
READABLE_RULES = ((4, 12000, 10000), (4, 9000, 6000))


def load(path):
  steps = collections.defaultdict(lambda: collections.defaultdict(list))
  meta = {}
  with open(path, encoding='utf-8') as lines:
    for row in csv.DictReader(lines, delimiter='\t', quoting=csv.QUOTE_NONE):
      for field in INT_FIELDS:
        row[field] = int(row[field])
      row['delta'] = row['cost_kana'] - row['cost_english']
      meta[row['input']] = (row['set'], row['keys'], row['display'])
      steps[row['input']][row['typed']].append(row)
  progress = collections.defaultdict(dict)
  if os.path.exists(path + '.progress'):
    with open(path + '.progress', encoding='utf-8') as lines:
      for row in csv.DictReader(lines, delimiter='\t', quoting=csv.QUOTE_NONE):
        progress[int(row['input'])][int(row['typed'])] = int(row['begin'])
  return steps, meta, progress


def is_english(candidate, body_length):
  if candidate['word'] == '-':
    return False
  if not candidate['reads']:
    return candidate['delta'] > -UNREADABLE_MARGIN
  if candidate['end'] == body_length and candidate['word_is_prefix']:
    return False  # A longer Japanese word may still be coming.
  length = candidate['end'] - candidate['begin']
  return any(length >= letters and candidate['cost_word'] > word_cost and
             candidate['delta'] > -margin for letters, word_cost, margin in READABLE_RULES)


def choose(candidates, typed, in_progress_begin):
  """The English runs to show after |typed| keys: [(begin, end)]."""
  body_length = typed - candidates[0]['tail']
  runs = []
  # From the left, the best run that starts first, then on after it.
  for candidate in sorted((c for c in candidates if is_english(c, body_length)),
                          key=lambda c: (c['begin'], -c['delta'])):
    if not runs or candidate['begin'] >= runs[-1][1]:
      runs.append((candidate['begin'], candidate['end']))
  if in_progress_begin is not None:
    runs = [run for run in runs if run[1] <= in_progress_begin]
    runs.append((in_progress_begin, typed))
  return runs


def display(keys, runs):
  out = []
  position = 0
  for begin, end in runs:
    out.append(romaji.to_kana(keys[position:begin]))
    out.append(keys[begin:end])
    position = end
  out.append(romaji.to_kana(keys[position:]))
  return ''.join(out)


def expected_runs(keys, shown):
  """Where the English words of the expected display are in the keys."""
  runs = []
  position = 0
  for word in re.findall(r'[a-z]{2,}', shown):
    begin = keys.find(word, position)
    if begin < 0:
      break
    runs.append((begin, begin + len(word)))
    position = begin + len(word)
  return runs


def main():
  steps, meta, progress = load(sys.argv[1])
  stats = collections.defaultdict(collections.Counter)
  examples = collections.defaultdict(list)
  for index, by_typed in steps.items():
    name, keys, shown = meta[index]
    wanted = expected_runs(keys, shown)
    counter = stats[name]
    counter['inputs'] += 1
    flicker = []
    final = None
    for typed in sorted(by_typed):
      runs = choose(by_typed[typed], typed, progress[index].get(typed))
      counter['keys'] += 1
      # English on screen that is not (the beginning of) a wanted word.
      wrong = [keys[b:e] for b, e in runs
               if not any(b == wb and e <= we for wb, we in wanted)]
      if wrong:
        flicker.append((typed, wrong))
      if typed == len(keys):
        final = display(keys, runs)
    if final is not None and final != shown:
      counter['final_wrong'] += 1
      examples[name, 'final'].append((keys, final, shown))
    elif flicker:
      counter['flicker_inputs'] += 1
      counter['flicker_keys'] += len(flicker)
      examples[name, 'flicker'].append((keys, flicker[:3]))
  print(f'{"set":20s} {"inputs":>7s} {"final wrong":>16s} {"flicker inputs":>18s} {"flicker keys":>13s}')
  for name in sorted(stats):
    c = stats[name]
    print(f'{name:20s} {c["inputs"]:7d} {c["final_wrong"]:8d} ({100 * c["final_wrong"] / c["inputs"]:5.2f}%)'
          f' {c["flicker_inputs"]:9d} ({100 * c["flicker_inputs"] / c["inputs"]:5.2f}%)'
          f' {100 * c["flicker_keys"] / max(1, c["keys"]):11.2f}%')
  if '--examples' in sys.argv:
    for key in sorted(examples):
      print(key, len(examples[key]))
      for example in examples[key][:12]:
        print('   ', example)


if __name__ == '__main__':
  main()
