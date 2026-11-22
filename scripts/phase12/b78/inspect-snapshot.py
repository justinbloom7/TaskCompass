#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline schema-2 backup checks; never executes SQL or prints private values."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('guiEvidence', ROOT / 'scripts/phase12/b77/validate-evidence.py')
evidence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evidence)
MANIFEST = json.loads((ROOT / 'migrations/mariadb/SchemaManifest.json').read_text(encoding='utf-8'))['2']


TOKEN = re.compile(r'(?:0[xX][0-9a-fA-F]+|-?[0-9]+|[A-Za-z_@][A-Za-z0-9_@$]*|.)')


def tokens(sql):
   result = []
   index = 0
   while index < len(sql):
      char = sql[index]
      if char.isspace():
         index += 1
      elif sql.startswith('--', index) and (index + 2 == len(sql) or sql[index + 2].isspace()) or char == '#':
         end = sql.find('\n', index); index = len(sql) if end < 0 else end + 1
      elif sql.startswith('/*', index):
         end = sql.find('*/', index + 2)
         evidence.require(end >= 0, 'Unterminated SQL comment')
         body = sql[index + 2:end]
         if body.startswith('!') or body.startswith('M!'):
            body = re.sub(r'^(?:M)?!\d*\s*', '', body)
            if body.strip() != r'\- enable the sandbox mode':
               result.extend(tokens(body))
         index = end + 2
      elif char in "'\"`":
         quote = char; kind = 'id' if quote == '`' else 'string'; value = ''; index += 1
         while index < len(sql):
            char = sql[index]; index += 1
            if char == quote:
               if index < len(sql) and sql[index] == quote:
                  value += quote; index += 1
               else:
                  break
            elif char == '\\' and quote != '`':
               evidence.require(index < len(sql), 'Unterminated SQL escape')
               char = sql[index]; index += 1
               value += {'0': '\0', 'b': '\b', 'n': '\n', 'r': '\r', 't': '\t', 'Z': '\x1a'}.get(char, char)
            else:
               value += char
         else:
            raise ValueError('Unterminated SQL literal')
         result.append((kind, value))
      else:
         match = TOKEN.match(sql, index)
         value = match[0]; index += len(value)
         result.append(('word', value))
   return result


def checkDump(path, data):
   statements = []; current = []
   for token in tokens(Path(path).read_text(encoding='utf-8')):
      if token == ('word', ';'):
         if current: statements.append(current)
         current = []
      else: current.append(token)
   evidence.require(not current, 'Unterminated dump statement')
   tables = {name.lower(): name for name in evidence.TABLES}
   created = set(); dumped = {name: {} for name in evidence.TABLES}
   for statement in statements:
      words = [value.upper() for kind, value in statement if kind == 'word']
      evidence.require(bool(words), 'Missing dump statement keyword')
      # Dump transaction boundary only; never executes SQL or accepts variants.
      if len(statement) == 1 and statement[0][0] == 'word' and words == ['COMMIT']:
         continue
      if words[0] == 'SET':
         evidence.require(not {word.lstrip('@') for word in words} & {'GLOBAL', 'PERSIST', 'PERSIST_ONLY', 'OUTFILE', 'DUMPFILE', 'SELECT'}, 'Unsafe SET statement')
         evidence.require(not any(kind == 'string' and 'NO_BACKSLASH_ESCAPES' in value.upper() for kind, value in statement), 'Unsupported SQL escape mode')
         continue
      evidence.require(words[0] in ('CREATE', 'INSERT'), 'Unexpected dump statement; private review required')
      evidence.require(len(statement) > 3 and statement[1][1].upper() == ('TABLE' if words[0] == 'CREATE' else 'INTO'), 'Unexpected dump target syntax')
      name = tables.get(statement[2][1].lower())
      evidence.require(name is not None, 'Unexpected dump table')
      columns = MANIFEST[name]['columns']
      if words[0] == 'CREATE':
         evidence.require(name not in created and statement[3][1] == '(', 'Duplicate/invalid CREATE TABLE')
         declared = []; depth = 0; beginning = False
         for kind, value in statement[3:]:
            if value == '(' and kind == 'word':
               depth += 1
               if depth == 1: beginning = True
            elif value == ')' and kind == 'word': depth -= 1
            elif value == ',' and depth == 1 and kind == 'word': beginning = True
            elif beginning:
               if kind == 'id': declared.append(value.lower())
               beginning = False
         evidence.require(declared == [field['name'].lower() for field in columns], 'Dump column order/coverage differs')
         evidence.require('INNODB' in words, 'Dump table is not InnoDB')
         created.add(name); continue
      evidence.require(name in created and statement[3][1].upper() == 'VALUES', 'Unsupported INSERT syntax/order')
      index = 4
      while index < len(statement):
         evidence.require(statement[index][1] == '(', 'Invalid INSERT tuple')
         index += 1; values = []
         for field in columns:
            evidence.require(index < len(statement), 'Short INSERT tuple')
            kind, value = statement[index]; index += 1
            if kind == 'word' and value.upper() == 'NULL': encoded = 'N'
            elif field['type'].upper().startswith(('INT', 'BIGINT')):
               evidence.require(kind == 'word' and re.fullmatch(r'-?\d+', value), 'Invalid numeric dump value')
               encoded = 'I' + str(int(value))
            elif kind == 'word' and value.lower().startswith('0x'):
               encoded = 'H' + value[2:].upper()
            else:
               evidence.require(kind == 'string', 'Unsupported dump literal')
               encoded = 'H' + value.encode('utf-8').hex().upper()
            values.append(encoded)
            expected = ')' if len(values) == len(columns) else ','
            evidence.require(index < len(statement) and statement[index][1] == expected, 'Invalid tuple column count')
            index += 1
         decoded = dict(zip([field['name'] for field in columns], map(evidence.decoded, values)))
         key = decoded.get('id', decoded.get('version'))
         evidence.require(key not in dumped[name], 'Duplicate dump record')
         dumped[name][key] = tuple(values)
         if index < len(statement):
            evidence.require(statement[index][1] == ',', 'Unexpected INSERT suffix'); index += 1
   evidence.require(created == set(evidence.TABLES), 'Dump does not contain exactly nine tables')
   for name in evidence.TABLES:
      evidence.require(dumped[name] == {key: row[0] for key, row in data[name].items()}, 'Dump data differs: ' + name)
   print('PASS: dump contains exactly nine table definitions and every exported value, including historical payloads.')


def preservation(baseline, current, platform):
   differences = []
   for name in evidence.TABLES:
      for key, (raw, before) in baseline[name].items():
         evidence.require(key in current[name], 'Protected row missing: ' + name)
         if current[name][key][0] != raw:
            fields = [field for field in before if before[field] != current[name][key][1][field]]
            evidence.require(platform == 'linux' and name == 'Task' and key == '24dd11c3-59df-4e23-ac21-422bcf2a1e3a'
               and set(fields) <= {'updatedAt', 'localRevision', 'objectiveId'}, 'Unexpected protected change: ' + name)
            differences.append(name + '/' + key + ': ' + ', '.join(fields))
   for name in ('Dataset', 'SchemaMigration', 'MigrationAttempt'):
      evidence.require(current[name] == baseline[name], 'Protected metadata differs: ' + name)
   print('Linux B7.7 preservation/restart exceptions remain UNRESOLVED; this backup does not waive them.' if platform == 'linux'
         else 'PASS: all protected B7.5 rows unchanged.')
   for difference in differences: print('EXCEPTION OBSERVED: ' + difference)
   for name in evidence.TABLES: print(name + '\t' + str(len(current[name])))


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('--data'); parser.add_argument('--dataset', required=True)
   parser.add_argument('--health-stdin', action='store_true')
   parser.add_argument('--dump'); parser.add_argument('--baseline'); parser.add_argument('--platform', choices=('linux', 'windows'))
   args = parser.parse_args()
   try:
      if args.health_stdin:
         source = (ROOT / 'scripts/phase12/b7/schema2-health.sql').read_text(encoding='utf-8')
         expected = re.findall(r"^SELECT '([^']+)' AS ruleName,COUNT", source, flags=re.M)
         actual = []
         for line in sys.stdin.buffer.read().decode('utf-8').splitlines():
            parts = line.split('\t')
            evidence.require(len(parts) == 2 and parts[1] == '0', 'Nonzero/malformed health result')
            actual.append(parts[0])
         evidence.require(bool(expected) and sorted(actual) == sorted(expected), 'Missing/extra CHECK/FK health rows')
         print('PASS: complete current schema2 health rule set is zero.'); return 0
      evidence.require(bool(args.data), 'Full data export required')
      data = evidence.readExport(args.data, MANIFEST)
      evidence.require(set(data['Dataset']) == {args.dataset}, 'Dataset identity differs')
      if args.baseline:
         preservation(evidence.readExport(args.baseline, MANIFEST), data, args.platform)
      if args.dump: checkDump(args.dump, data)
   except (OSError, ValueError, KeyError, TypeError, UnicodeError, IndexError) as error:
      # Only our static invariant messages expose detail; never print dump values.
      detail = str(error) if type(error) is ValueError else 'Malformed or inaccessible evidence'
      print('STOP: ' + detail + '; preserve files for review.'); return 1
   return 0


if __name__ == '__main__':
   raise SystemExit(main())
