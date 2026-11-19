#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('environmentReport', ROOT / 'scripts/environment-report.py')
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)


class EnvironmentReportTests(unittest.TestCase):
   def testChangeHistoryDoesNotGrowOnUnchangedFields(self):
      with tempfile.TemporaryDirectory() as directory:
         output = Path(directory) / 'reports'
         destination = Path(directory) / 'qa'
         destination.mkdir()
         first, changed = REPORT.saveReport({'version': '1'}, output, 'build', destination)
         self.assertEqual(changed, ['version'])
         oldNotes = (output / 'BuildNotes.md').read_text(encoding="utf-8")
         second, changed = REPORT.saveReport({'version': '1', 'server_evidence': 'new provenance path'}, output, 'launch', destination)
         self.assertEqual(changed, [])
         self.assertEqual(oldNotes, (output / 'BuildNotes.md').read_text(encoding="utf-8"))
         self.assertNotEqual(first, second)
         _, changed = REPORT.saveReport({'version': '2'}, output, 'build', destination)
         self.assertEqual(changed, ['version'])
         self.assertIn('1 → 2', (output / 'BuildNotes.md').read_text(encoding="utf-8"))
         self.assertEqual(len(list(destination.glob('*.sha256'))), 3)

   def testBuildCacheQtPathsWithSpacesAndOnlyWhitelistedValues(self):
      with tempfile.TemporaryDirectory(prefix='report spaces ') as directory:
         path = Path(directory)
         package = path / 'Qt package'
         package.mkdir()
         (package / 'Qt6WidgetsConfigVersionImpl.cmake').write_text('set(PACKAGE_VERSION "6.12.0")')
         (path / 'CMakeCache.txt').write_text('Qt6Widgets_DIR:PATH=' + str(package) + '\nPASSWORD:STRING=do-not-report\n')
         self.assertNotIn('PASSWORD', REPORT.cacheValues(path))
         self.assertEqual(REPORT.qtVersion(package, 'Qt6Widgets'), '6.12.0')

   def testNoDatabaseQueryAndSavedServerEvidenceLabel(self):
      with tempfile.TemporaryDirectory() as directory:
         path = Path(directory)
         server = path / 'server.tsv'
         server.write_text('SERVER\tfeldria-w10\t13.0.2-MariaDB\t3306\ttaskcompass_dev\n')
         calls = []
         def fakeCommand(arguments, cwd):
            calls.append(arguments)
            return b''
         with patch.object(REPORT, 'command', side_effect=fakeCommand):
            fields = REPORT.collect(path, path, server)
         self.assertIn('13.0.2-MariaDB', fields['server'])
         self.assertIn('saved verified metadata', fields['server_evidence'])
         self.assertTrue(all(call[0] in ('git', 'cmake', '/usr/bin/c++', '/usr/bin/g++') or call[-1] == '--version'
                             for call in calls))
         self.assertFalse(any('--connect' in call or '--execute' in call for call in calls))

   def testUncommittedSourceContentChangesFingerprint(self):
      with tempfile.TemporaryDirectory() as directory:
         path = Path(directory)
         (path / 'src').mkdir()
         source = path / 'src/Example.cpp'
         source.write_text('first')
         def fakeCommand(arguments, cwd):
            if arguments[1] == 'ls-files':
               return b'src/Example.cpp\0'
            if arguments[1] == 'status':
               return b' M src/Example.cpp\0'
            return b'unchanged-commit\n'
         with patch.object(REPORT, 'command', side_effect=fakeCommand):
            first = REPORT.sourceState(path)
            source.write_text('second')
            second = REPORT.sourceState(path)
         self.assertEqual(first['git_commit'], second['git_commit'])
         self.assertTrue(second['git_dirty'])
         self.assertNotEqual(first['source_sha256'], second['source_sha256'])

   def testMissingGitDoesNotClaimCleanCheckout(self):
      with patch.object(REPORT, 'command', return_value=b''):
         result = REPORT.sourceState(ROOT)
      self.assertEqual(result['git_commit'], 'unavailable')
      self.assertEqual(result['git_dirty'], 'unavailable')
      self.assertEqual(result['source_sha256'], 'unavailable')

   def testSecretEnvironmentRemovedFromSubprocesses(self):
      with patch.dict(os.environ, {'TASKCOMPASS_DB_PASSWORD': 'private-password', 'MYSQL_PWD': 'other-secret'}):
         with patch.object(REPORT.subprocess, 'run') as execute:
            execute.return_value.returncode = 0
            execute.return_value.stdout = b'version'
            REPORT.command(['cmake', '--version'], ROOT)
            environment = execute.call_args.kwargs['env']
            self.assertNotIn('TASKCOMPASS_DB_PASSWORD', environment)
            self.assertNotIn('MYSQL_PWD', environment)


if __name__ == '__main__':
   unittest.main()
