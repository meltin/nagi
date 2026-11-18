#!/usr/bin/env python3
"""Measures the IME's own detector: types every input of prefix_collect.py key by key
through english_detector_main and counts wrong final displays and flicker.

  python3 tools/english/measure/run_detector.py [--examples] [detector flags...]

Build first: (cd src && bazelisk build //english:english_detector_main --config release_build)
"""
import collections
import os
import random
import re
import subprocess
import sys

import candidates
import prefix_collect

ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..')
TOOL = os.path.join(ROOT, 'src', 'bazel-bin', 'english', 'english_detector_main')


def english_runs(display):
  return re.findall(r'[a-z]{3,}', display)


def main():
  flags = [arg for arg in sys.argv[1:] if arg.startswith('--') and arg != '--examples']
  words = candidates.load_words()
  # Commas are typed between clauses; the detector looks at letters and "-" only.
  inputs = [(name, keys.replace(',', ''), wanted.replace('、', ''))
            for name, keys, wanted in prefix_collect.inputs(words, random.Random(20261118))]
  inputs = [item for item in inputs if re.fullmatch(r'[a-z-]+', item[1])]
  # NAGI_SETS=held_out_mixed,japanese_text runs only those sets; NAGI_LIMIT=1000 runs
  # at most that many inputs of each set. For a quick look while tuning.
  if os.environ.get('NAGI_SETS'):
    inputs = [item for item in inputs if item[0] in os.environ['NAGI_SETS'].split(',')]
  if os.environ.get('NAGI_LIMIT'):
    seen = collections.Counter()
    limited = []
    for item in inputs:
      seen[item[0]] += 1
      if seen[item[0]] <= int(os.environ['NAGI_LIMIT']):
        limited.append(item)
    inputs = limited
  result = subprocess.run([TOOL, '--words=' + candidates.WORDS_PATH] + flags,
                          input='\n'.join(keys for _, keys, _ in inputs) + '\n',
                          capture_output=True, text=True, check=True)
  stats = collections.defaultdict(collections.Counter)
  examples = collections.defaultdict(list)
  for (name, keys, wanted), line in zip(inputs, result.stdout.splitlines()):
    fields = line.split('\t')
    assert fields[0] == keys, (fields[0], keys)
    displays = fields[1:]
    counter = stats[name]
    counter['inputs'] += 1
    counter['keys'] += len(displays)
    wanted_words = english_runs(wanted)
    # English on screen that is not (the beginning of) a wanted word.
    flicker = [(n + 1, run) for n, shown in enumerate(displays) for run in english_runs(shown)
               if not any(word.startswith(run) for word in wanted_words)]
    if displays[-1] != wanted:
      counter['final_wrong'] += 1
      examples[name, 'final'].append((keys, displays[-1], wanted))
    elif flicker:
      counter['flicker_inputs'] += 1
      counter['flicker_keys'] += len({n for n, _ in flicker})
      examples[name, 'flicker'].append((keys, flicker[:3]))
  print(f'{"set":20s} {"inputs":>7s} {"final wrong":>16s} {"flicker inputs":>18s} {"flicker keys":>13s}')
  for name in sorted(stats):
    c = stats[name]
    print(f'{name:20s} {c["inputs"]:7d} {c["final_wrong"]:8d} ({100 * c["final_wrong"] / c["inputs"]:5.2f}%)'
          f' {c["flicker_inputs"]:9d} ({100 * c["flicker_inputs"] / c["inputs"]:5.2f}%)'
          f' {100 * c["flicker_keys"] / max(1, c["keys"]):11.2f}%')
  print(result.stderr.strip().splitlines()[-1])
  if '--examples' in sys.argv:
    for key in sorted(examples):
      print(key, len(examples[key]))
      for example in examples[key][:14]:
        print('   ', example)


if __name__ == '__main__':
  main()
