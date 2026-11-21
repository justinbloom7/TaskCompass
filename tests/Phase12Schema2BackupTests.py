#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import copy
import importlib.util
import io
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
from contextlib import redirect_stdout

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
   spec = importlib.util.spec_from_file_location(name, ROOT / path)
   result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result); return result


CHECK = module('backupInspector', 'scripts/phase12/b78/inspect-snapshot.py')
FIXTURE = module('backupFixture', 'tests/GuiAcceptanceEvidenceTests.py')


def dump(data):
   result = [r'/*M!999999\- enable the sandbox mode */', '/*!40101 SET NAMES utf8mb4 */;']
   for name in CHECK.evidence.TABLES:
      columns = CHECK.MANIFEST[name]['columns']
      result.append('CREATE TABLE `' + name.lower() + '` (' + ','.join('`' + field['name'] + '` ' + field['type'] for field in columns) + ') ENGINE=InnoDB;')
      for row in data.get(name, []):
         def literal(value):
            if value is None: return 'NULL'
            if isinstance(value, int): return str(value)
            return "'" + value.replace('\\', '\\\\').replace("'", "\\'").replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t').replace('\0', '\\0') + "'"
         result.append('INSERT INTO `' + name.lower() + '` VALUES (' + ','.join(literal(row[field['name']]) for field in columns) + ');')
   return '\n'.join(result) + '\n'


class Schema2BackupTests(unittest.TestCase):
   def setUp(self):
      self.directory = tempfile.TemporaryDirectory(); self.addCleanup(self.directory.cleanup)
      self.root = Path(self.directory.name)
      self.data = FIXTURE.qaRecords(FIXTURE.baseline())
      self.data['Task'][0]['notes'] = "quotes ' \\ newline\nCR\r\ntab\t snowman ☃ DROP TABLE"
      (self.root / 'data.tsv').write_bytes(FIXTURE.export(self.data).encode('utf-8'))
      self.rows = CHECK.evidence.readExport(self.root / 'data.tsv', CHECK.MANIFEST)

   def inspect(self, text):
      path = self.root / 'dump.sql'; path.write_bytes(text.encode('utf-8'))
      with redirect_stdout(io.StringIO()): CHECK.checkDump(path, self.rows)

   def testCompleteUnicodePayloadsAndNativeLineEndings(self):
      self.inspect(dump(self.data))
      self.inspect(dump(self.data).replace('\n', '\r\n'))

   def testExtendedInsertsAndConstraints(self):
      text = dump(self.data)
      rows = text.splitlines()
      inserts = [line for line in rows if line.startswith('INSERT INTO `task` VALUES ')]
      merged = 'INSERT INTO `task` VALUES ' + ','.join(line.split(' VALUES ', 1)[1][:-1] for line in inserts) + ';'
      rows = [line for line in rows if not line.startswith('INSERT INTO `task` VALUES ')]
      # Insert after all CREATE statements; table creation order need not follow rows.
      rows.append(merged)
      text = '\n'.join(rows) + '\n'
      text = text.replace(') ENGINE=InnoDB;', ', PRIMARY KEY (`id`), CONSTRAINT `sample` CHECK (1=1)) ENGINE=InnoDB;')
      self.inspect(text)

   def testMissingTableRowsAndChangedHistoricalPayloadRefused(self):
      for table in ('Objective', 'CommandRecord', 'LocalChange', 'MigrationAttempt'):
         altered = copy.deepcopy(self.data); altered[table] = []
         with self.assertRaises(ValueError): self.inspect(dump(altered))
      changed = copy.deepcopy(self.data); changed['CommandRecord'][0]['afterSnapshot'] = '{"changed":true}'
      with self.assertRaises(ValueError): self.inspect(dump(changed))
      with self.assertRaises(ValueError): self.inspect(dump(self.data).replace('CREATE TABLE `dataset`', 'CREATE TABLE `other`'))

   def testUnsafeOrUnsupportedStatementsNeverReplayed(self):
      for statement in ['DROP TABLE Task;', 'CREATE DATABASE other;', 'GRANT SELECT ON x.* TO y;',
                        'SET GLOBAL foreign_key_checks=0;', '/*!50000 DROP TABLE Task */;',
                        "SET SQL_MODE='NO_BACKSLASH_ESCAPES';", 'SELECT 1 INTO OUTFILE \'x\';']:
         with self.assertRaises(ValueError): self.inspect(dump(self.data) + statement)

   def testHealthStreamingRequiresEveryZeroRule(self):
      source = (ROOT / 'scripts/phase12/b7/schema2-health.sql').read_text(encoding='utf-8')
      rules = CHECK.re.findall(r"^SELECT '([^']+)' AS ruleName,COUNT", source, flags=CHECK.re.M)
      good = ''.join(rule + '\t0\n' for rule in rules)
      variants = [(good, 0), (good.replace('\n', '\r\n'), 0), (good.replace('\t0', '\t1', 1), 1),
                  ('\n'.join(good.splitlines()[1:]) + '\n', 1), (good + 'extra\t0\n', 1)]
      for data, expected in variants:
         result = subprocess.run([sys.executable, str(ROOT / 'scripts/phase12/b78/inspect-snapshot.py'),
                                  '--dataset', FIXTURE.DATASET, '--health-stdin'], input=data.encode('utf-8'),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
         self.assertEqual(result.returncode, expected)

   def testKnownLinuxExceptionDoesNotAllowAnyOtherOriginalChange(self):
      before = self.rows; after = copy.deepcopy(before)
      key = next(iter(after['Task'])); row = after['Task'].pop(key)
      exception = '24dd11c3-59df-4e23-ac21-422bcf2a1e3a'
      baseline = copy.deepcopy(before); baseline['Task'][exception] = baseline['Task'].pop(key)
      fields = dict(row[1]); fields['objectiveId'] = FIXTURE.identity(12)
      after['Task'][exception] = (('different',), fields)
      with redirect_stdout(io.StringIO()) as output: CHECK.preservation(baseline, after, 'linux')
      self.assertIn('UNRESOLVED', output.getvalue())
      with self.assertRaises(ValueError): CHECK.preservation(baseline, after, 'windows')
      fields['title'] = 'unexpected'
      with self.assertRaises(ValueError): CHECK.preservation(baseline, after, 'linux')


if __name__ == '__main__': unittest.main()
