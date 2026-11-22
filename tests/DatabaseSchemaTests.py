#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cumulative diagram reader/artifact tests; no renderer, SQL or network."""
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('schema', ROOT / 'docs/diagrams/GenerateDatabaseSchema.py')
SCHEMA = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCHEMA)


class DatabaseSchemaTests(unittest.TestCase):
   def setUp(self):
      self.sources = [p.read_text(encoding="utf-8") for p in SCHEMA.MIGRATIONS]
      self.tables = SCHEMA.parseMigrations(self.sources)

   def testCumulativeKeysAndNullability(self):
      self.assertEqual(len(self.tables), 9)
      self.assertEqual(sum(len(t['columns']) for t in self.tables.values()), 85)
      self.assertEqual(sum(len(t['foreignKeys']) for t in self.tables.values()), 20)
      for name in ['CommandRecord', 'LocalChange']:
         columns = {c['name']: c for c in self.tables[name]['columns']}
         for field in ['taskId', 'pillarId', 'projectId', 'objectiveId']:
            self.assertTrue(columns[field]['nullable'])
            self.assertIn('FK', columns[field]['keys'])
      self.assertNotIn('commandTask', [k[0] for k in self.tables['CommandRecord']['foreignKeys']])
      self.assertIn('commandTaskV2', [k[0] for k in self.tables['CommandRecord']['foreignKeys']])
      self.assertNotIn('commandPayload', [k[0] for k in self.tables['CommandRecord']['checks']])

   def testUnsupportedSyntaxFails(self):
      for statement in ['TRUNCATE Task;', 'ALTER TABLE Task DROP COLUMN title;', 'SELECT 1;']:
         with self.assertRaises(ValueError):
            SCHEMA.parseMigrations(self.sources + [statement])
      with self.assertRaises(ValueError):
         SCHEMA.splitSql("CREATE TABLE Broken (name VARCHAR(2);", ';')

   def testRegistryMatchesBothSourceVersions(self):
      sources = [path.read_text(encoding="utf-8") for path in SCHEMA.MIGRATIONS]
      expected = {str(version): SCHEMA.parseMigrations(sources[:version]) for version in [1, 2]}
      self.assertEqual(json.loads((SCHEMA.MIGRATIONS[0].parent / 'SchemaManifest.json').read_text(encoding="utf-8")), json.loads(json.dumps(expected)))

   def testCheckIsIndependentOfWindowsDefaultEncoding(self):
      readText = Path.read_text
      def readWithLegacyDefault(path, *arguments, **options):
         options.setdefault('encoding', 'cp1252')
         return readText(path, *arguments, **options)
      # Exercise the real checker with Windows-style implicit text decoding.
      with patch.object(Path, 'read_text', readWithLegacyDefault), \
           patch('sys.argv', ['GenerateDatabaseSchema.py', '--check']):
         SCHEMA.main()

   def testArtifactsMatchCumulativeSource(self):
      self.assertEqual((SCHEMA.DIRECTORY / 'DatabaseSchema.md').read_text(encoding="utf-8"), SCHEMA.markdown(self.tables))
      SCHEMA.validateArtifacts(self.tables, (SCHEMA.DIRECTORY / 'DatabaseSchema.svg').read_text(encoding="utf-8"))


if __name__ == '__main__':
   unittest.main()
