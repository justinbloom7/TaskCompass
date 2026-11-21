#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import copy
import importlib.util
import json
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('guiEvidence', ROOT / 'scripts/phase12/b77/validate-evidence.py')
EVIDENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVIDENCE)
MANIFEST = json.loads((ROOT / 'migrations/mariadb/SchemaManifest.json').read_text(encoding="utf-8"))['2']
DATASET = '21be9da7-b767-4578-b972-cc9a35122ddb'
PREFIX = 'B77-QA-linux-offline'


def identity(number):
   return '00000000-0000-0000-0000-' + format(number, '012x')


def record(table, number, **overrides):
   result = {}
   for field in MANIFEST[table]['columns']:
      name = field['name']
      if field['nullable']:
         result[name] = None
      elif name in ('id', 'operationId'):
         result[name] = identity(number)
      elif name == 'datasetId':
         result[name] = DATASET
      elif field['type'].upper().startswith(('INT', 'BIGINT')):
         result[name] = 1
      else:
         result[name] = 'value'
   result.update(overrides)
   return result


def token(value):
   if value is None:
      return 'N'
   if isinstance(value, int):
      return 'I' + str(value)
   return 'H' + value.encode('utf-8').hex().upper()


def export(records):
   lines = []
   for table in EVIDENCE.TABLES:
      lines.append('COUNT:' + table + '\t' + str(len(records.get(table, []))))
      for values in records.get(table, []):
         lines.append(table + '\t' + '\t'.join(token(values[field['name']]) for field in MANIFEST[table]['columns']))
   return '\n'.join(lines) + '\n'


def baseline():
   return {
      'Dataset': [record('Dataset', 1, id=DATASET)],
      'SchemaMigration': [record('SchemaMigration', 1, version=1), record('SchemaMigration', 2, version=2)],
      'MigrationAttempt': [record('MigrationAttempt', 1, version=2)],
      'CommandRecord': [record('CommandRecord', 4, taskId=identity(2), afterSnapshot='{ \"legacy\": 1 }', payloadVersion=1, operationId=identity(5))],
      'LocalChange': [record('LocalChange', 6, taskId=identity(2), immutablePayload='{ \"legacy\": 1 }', payloadVersion=1, operationId=identity(5), acknowledgmentState='pending')],
      'Task': [record('Task', 2, title='Original\tUnicode \u2603', lifecycleStatus='completed', completedAt=2),
               record('Task', 3, title='Original tombstone', deletedAt=2)]}


def qaRecords(original):
   current = copy.deepcopy(original)
   current['Pillar'] = [record('Pillar', 10, name=PREFIX + ' Root')]
   current['Project'] = [record('Project', 11, title=PREFIX + ' Project', pillarId=identity(10))]
   current['Objective'] = [record('Objective', 12, title=PREFIX + ' Outcome', projectId=identity(11))]
   current['Task'].append(record('Task', 13, title=PREFIX + ' Task', lifecycleStatus='active', objectiveId=identity(12)))
   snapshot = json.dumps({'id': identity(13), 'title': PREFIX + ' Task', 'entityType': 'task',
                          'datasetId': DATASET, 'localRevision': '1', 'updatedAt': '1'})
   current['CommandRecord'] += [record('CommandRecord', 14, taskId=identity(13), afterSnapshot=snapshot,
                                    payloadVersion=2, operationId=identity(15), resultingRevision=1)]
   current['LocalChange'] += [record('LocalChange', 16, taskId=identity(13), immutablePayload=snapshot,
                                  payloadVersion=2, operationId=identity(15), acknowledgmentState='pending')]
   return current


class GuiAcceptanceEvidenceTests(unittest.TestCase):
   def testInspectionReportsOnlyChangedFieldNames(self):
      spec = importlib.util.spec_from_file_location('inspection', ROOT / 'scripts/phase12/b77/inspect-protected.py')
      inspection = importlib.util.module_from_spec(spec); spec.loader.exec_module(inspection)
      before = {name: {} for name in EVIDENCE.TABLES}
      after = copy.deepcopy(before)
      before['Task']['id'] = (('old',), {'title': 'private-before', 'localRevision': 1})
      after['Task']['id'] = (('new',), {'title': 'private-after', 'localRevision': 2})
      self.assertEqual(inspection.differences(before, after), [('Task', 'id', ['title', 'localRevision'])])
      after['Task'].clear()
      self.assertEqual(inspection.differences(before, after), [('Task', 'id', ['MISSING'])])
      self.assertEqual(inspection.differences(before, before), [])

   def setUp(self):
      self.directory = tempfile.TemporaryDirectory()
      self.addCleanup(self.directory.cleanup)
      self.original = baseline()
      self.current = qaRecords(self.original)

   def parsed(self, records):
      path = Path(self.directory.name) / 'export.tsv'
      path.write_text(export(records), encoding='utf-8')
      return EVIDENCE.readExport(path, MANIFEST)

   def checkCurrent(self):
      return EVIDENCE.validate(self.parsed(self.original), self.parsed(self.current), PREFIX, DATASET)

   def testSelectOrderMatchesActualSchemaManifest(self):
      actual = {}
      table = None
      source = (ROOT / 'scripts/phase12/b7/schema2-export.sql').read_text(encoding="utf-8")
      for line in source.splitlines():
         match = re.match(r"SELECT '([^']+)' AS tableName,", line)
         if match:
            table = match.group(1)
            actual[table] = []
         elif line.startswith('   IF('):
            field = re.search(r' AS ([A-Za-z][A-Za-z0-9_]*),?$', line)
            self.assertIsNotNone(field)
            actual[table].append(field.group(1))
      self.assertEqual(actual, {name: [field['name'] for field in MANIFEST[name]['columns']]
                                for name in EVIDENCE.TABLES})

   def testOriginalTombstoneCompletionUnicodeAndNewOwners(self):
      new, parents = self.checkCurrent()
      self.assertEqual(len(new['Task']), 1)
      self.assertEqual(parents[('Objective', identity(12))], ('Project', identity(11)))
      self.current['Pillar'][0]['archivedAt'] = 2
      new, parents = self.checkCurrent()
      path = Path(self.directory.name) / 'inventory.tsv'
      EVIDENCE.writeInventory(path, self.parsed(self.current), new, parents)
      self.assertIn(identity(13), path.read_text(encoding="utf-8"))
      self.assertIn('True', path.read_text(encoding="utf-8"))

   def testProtectedValuesAndPayloadBytesCannotChange(self):
      for field, value in [('title', 'changed'), ('completedAt', 99), ('deletedAt', None), ('objectiveId', identity(12))]:
         self.current = qaRecords(self.original)
         row = 1 if field == 'deletedAt' else 0
         self.current['Task'][row][field] = value
         with self.assertRaises(ValueError):
            self.checkCurrent()
      for table, field in [('CommandRecord', 'afterSnapshot'), ('LocalChange', 'immutablePayload')]:
         self.current = qaRecords(self.original)
         self.current[table][0][field] = '{"legacy":1}'
         with self.assertRaises(ValueError):
            self.checkCurrent()
      for table in ('SchemaMigration', 'MigrationAttempt', 'Dataset'):
         self.current = qaRecords(self.original)
         self.current[table].append(copy.deepcopy(self.current[table][0]))
         with self.assertRaises(ValueError):
            self.checkCurrent()

   def testNewNonQaRowsAndWritesToOriginalRejected(self):
      self.current['Task'][-1]['title'] = 'unapproved'
      with self.assertRaises(ValueError):
         self.checkCurrent()
      self.current = qaRecords(self.original)
      self.current['CommandRecord'][-1]['taskId'] = self.original['Task'][0]['id']
      with self.assertRaises(ValueError):
         self.checkCurrent()

   def testGraphCyclesAndContradictoryOrMissingOwnersRejected(self):
      self.current['Pillar'][0]['parentPillarId'] = identity(10)
      with self.assertRaises(ValueError):
         self.checkCurrent()
      self.current = qaRecords(self.original)
      self.current['Task'][-1]['directProjectId'] = identity(11)
      with self.assertRaises(ValueError):
         self.checkCurrent()
      self.current = qaRecords(self.original)
      self.current['Project'][0]['pillarId'] = identity(999)
      with self.assertRaises(ValueError):
         self.checkCurrent()

   def testAtomicHistoryOutboxPairRequired(self):
      for mutation in ('missing', 'payload', 'target', 'revision', 'snapshot'):
         self.current = qaRecords(self.original)
         if mutation == 'missing':
            self.current['LocalChange'] = []
         elif mutation == 'payload':
            self.current['LocalChange'][-1]['immutablePayload'] = '{}'
         elif mutation == 'target':
            self.current['LocalChange'][-1]['taskId'] = identity(999)
         elif mutation == 'revision':
            self.current['LocalChange'][-1]['localRevision'] = 5
         else:
            self.current['CommandRecord'][-1]['afterSnapshot'] = '{}'
            self.current['LocalChange'][-1]['immutablePayload'] = '{}'
         with self.assertRaises(ValueError):
            self.checkCurrent()

   def testMalformedMissingDuplicateExportsRejected(self):
      path = Path(self.directory.name) / 'bad.tsv'
      for text in ('', 'COUNT:Task\t1\n', export(self.current).replace('COUNT:Task\t3', 'COUNT:Task\t99'),
                   export(self.current) + 'COUNT:Task\t3\n'):
         path.write_text(text)
         with self.assertRaises(ValueError):
            EVIDENCE.readExport(path, MANIFEST)


if __name__ == '__main__':
   unittest.main()
