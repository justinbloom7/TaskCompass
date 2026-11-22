#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Inspect saved failed-checkpoint exports offline; print field names, never values."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('evidence', Path(__file__).with_name('validate-evidence.py'))
evidence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evidence)


def differences(baseline, current):
   result = []
   for table in evidence.TABLES:
      for key, (tokens, before) in baseline[table].items():
         after = current[table].get(key)
         if after is None:
            result.append((table, key, ['MISSING']))
         elif tokens != after[0]:
            result.append((table, key, [name for name, value in before.items() if after[1][name] != value]))
   return result


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('--qa-dir', required=True)
   parser.add_argument('--checkpoint', default='final')
   args = parser.parse_args()
   try:
      if not args.checkpoint or any(char not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for char in args.checkpoint):
         raise ValueError('Unsafe checkpoint label')
      root = Path(args.qa_dir)
      baseline = root / 'data.hex.tsv'
      seal = root / 'initial.sha256'
      current = root / args.checkpoint / 'data.hex.tsv'
      if any(path.is_symlink() for path in (root, baseline, seal, current.parent, current)):
         raise ValueError('Linked evidence requires review')
      expected = [line[:64] for line in seal.read_text(encoding='utf-8').splitlines() if line[66:] == 'data.hex.tsv']
      if len(expected) != 1 or hashlib.sha256(baseline.read_bytes()).hexdigest() != expected[0]:
         raise ValueError('Initial baseline checksum differs or is missing')
      manifest = json.loads((Path(__file__).resolve().parents[3] / 'migrations/mariadb/SchemaManifest.json').read_text(encoding='utf-8'))['2']
      before = evidence.readExport(baseline, manifest)
      after = evidence.readExport(current, manifest)
      print('Offline inspection of saved checkpoint only; it may be unsealed and is not a fresh database verification.')
      for table, key, fields in differences(before, after):
         print(table + '/' + str(key) + ': ' + ', '.join(fields))
      for table in evidence.TABLES:
         print(table + ' new rows: ' + str(len(set(after[table]) - set(before[table]))))
      print('No values printed or files changed. Preserve failed checkpoint; no acceptance, repair or retry authorized.')
   except (OSError, ValueError, KeyError, TypeError, UnicodeError):
      print('STOP: Evidence unavailable, malformed or baseline checksum differs. Preserve files; no repair attempted.')
      return 1
   return 0


if __name__ == '__main__':
   raise SystemExit(main())
