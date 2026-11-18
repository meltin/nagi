#!/usr/bin/env python3
"""Types the measured inputs into a real session with the automatic input mode on, and
compares what Enter commits with what the detector alone shows at the end. A difference
means the session and the detector read the keys differently.

  (cd src && bazelisk build //session:nagi_typing_main //english:english_detector_main \\
      --config release_build)
  NAGI_SETS=held_out_mixed,held_out_japanese python3 tools/english/measure/compare_session.py
"""

import os
import subprocess
import sys
import tempfile

import candidates
import run_detector

TYPIST = os.path.join(run_detector.ROOT, 'src', 'bazel-bin', 'session', 'nagi_typing_main')


def main():
  inputs = run_detector.load_inputs()
  text = '\n'.join(keys for _, keys, _ in inputs) + '\n'
  detector = subprocess.run([run_detector.TOOL, '--words=' + candidates.WORDS_PATH],
                            input=text, capture_output=True, text=True, check=True)
  with tempfile.TemporaryDirectory() as profile:
    session = subprocess.run([TYPIST, '--quiet', '--english', '--live=false', '--zenz=false',
                              '--profile=' + profile],
                             input=text, capture_output=True, text=True, check=True)
  different = 0
  for (name, keys, _), shown, committed in zip(inputs, detector.stdout.splitlines(),
                                               session.stdout.splitlines()):
    shown = shown.split('\t')[-1]
    assert committed.split('\t')[0] == keys, (committed, keys)
    committed = committed.split('\t')[1]
    # Enter commits an "n" still waiting at the end as it is, with or without this mode.
    if committed.endswith('ｎ'):
      committed = committed[:-1] + 'ん'
    if shown != committed:
      different += 1
      if different <= 20:
        print(name, keys, shown, committed, sep='\t')
  print(f'{len(inputs)} inputs, {different} different')
  return 1 if different else 0


if __name__ == '__main__':
  sys.exit(main())
