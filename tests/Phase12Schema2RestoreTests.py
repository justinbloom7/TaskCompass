#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Isolated B7.9 fixtures: never fall back to a real client, ACL tool or database."""
import argparse
import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]

def load(name, file):
   spec = importlib.util.spec_from_file_location(name, ROOT / file)
   result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result); return result

R = load('restore79', 'scripts/phase12/b79/restore.py')
F = load('restoreFixture79', 'tests/GuiAcceptanceEvidenceTests.py')
D = load('dumpFixture79', 'tests/Phase12Schema2BackupTests.py')
PASSWORD = "Offline-secret-☃'\\123"


class FakeSession:
   def __init__(self, runner, admin): self.runner = runner; self.admin = admin
   def close(self): pass
   def finish(self, operation): pass
   def query(self, sql, operation):
      r = self.runner; state = r.state
      if isinstance(sql, bytes): sql = sql.decode('utf-8')
      state['calls'].append(operation)
      if 'metadata ' in operation: return metadata(r)
      if operation == 'PUBLIC privilege check':
         return state.get('public', b'GRANT SELECT, INSERT ON `test`.* TO PUBLIC\r\nGRANT CREATE ON `test\\_%`.* TO PUBLIC\r\n')
      if operation == 'one-shot restore provisioning':
         assert "'" + PASSWORD.replace("'", "''") + "'" in sql
         assert state['policy'] == 'absent'
         state['policy'] = 'import'; state['mutations'].append('provision')
         if state.get('provisionFail'): raise ValueError('one-shot provisioning failed')
         return b''
      if operation == 'one-shot exact dump import':
         assert sql.encode('utf-8') == (r.snapshot / 'schema2.sql').read_bytes()
         assert state['policy'] == 'import' and not state['populated']
         state['mutations'].append('restore'); state['populated'] = True
         if state.get('importFail'): raise ValueError('one-shot import failed ERROR 1064')
         if state.get('interrupt'): raise ValueError('Interrupted; outcome uncertain')
         return b''
      if operation == 'one-shot revoke INSERT/CREATE':
         state['mutations'].append('readonly'); state['policy'] = 'readonly'; return b''
      if operation.startswith('restore '):
         filename = {'schema2-export.sql': 'source2-before.hex.tsv', 'ddl2-export.sql': 'source2-before.ddl.txt',
                     'schema2-health.sql': 'health-before.tsv'}[operation.split(' ', 1)[1]]
         raw = (r.snapshot / filename).read_bytes()
         if state.get('dataMismatch') and filename.endswith('hex.tsv'): raw += b'changed\n'
         if state.get('ddlMismatch') and filename.endswith('ddl.txt'): raw = raw.replace(b'InnoDB', b'MyISAM', 1)
         if state.get('badHealth') and filename.startswith('health'): raw = raw.replace(b'\t0', b'\t1', 1)
         return raw
      raise AssertionError('Missing fake query; never use real DB: ' + operation)


def metadata(r):
   state = r.state
   lines = [['SERVER', r.host, r.version, '3306', 'NULL', 'root@localhost', r.case, '1', '1'],
            ['EXTRA'] + ['0'] * 5, ['INVALID'] + ['0'] * 5]
   if state.get('wrongServer'): lines[0][1] = 'other'
   if state.get('extraAuthority'): lines[1][1] = '1'
   if state.get('crossSchema'): lines[2][4] = '1'
   if state.get('storedCode'): lines[2][1] = '1'
   if state.get('sessions'): lines[2][5] = '1'
   if state['policy'] != 'absent':
      lines += [['DATABASE', R.TARGET, 'utf8mb4', 'utf8mb4_unicode_ci'], ['ACCOUNT', R.TARGET, '127.0.0.1', 'N'], ['GLOBAL', 'USAGE', 'NO']]
      if state.get('hostVariant'): lines.append(['ACCOUNT', R.TARGET, 'localhost', 'N'])
      rights = ['SELECT'] if state['policy'] == 'readonly' else ['SELECT', 'INSERT', 'CREATE']
      if state.get('badGrant'): rights += ['DROP']
      lines += [['RIGHT', R.TARGET.replace('_', '\\_'), right, 'NO'] for right in rights]
      if state['populated']:
         lines += [['OBJECT', table.lower(), 'BASE TABLE', 'InnoDB'] for table in R.INSPECT.evidence.TABLES]
         lines += [['COLUMN', table.lower(), column['name']] for table, value in R.INSPECT.MANIFEST.items() for column in value['columns']]
         if state.get('missingColumn'): lines.pop()
         if state.get('extraObject'): lines.append(['OBJECT', 'unexpected', 'VIEW', 'NULL'])
   ending = '\r\n' if r.platform == 'windows' else '\n'
   return (ending.join('\t'.join(line) for line in lines) + ending).encode('utf-8')


class MockRestore(R.Restore):
   def __init__(self, args, state):
      super().__init__(args); self.state = state
   def connect(self, admin=True):
      result = FakeSession(self, admin)
      if admin: self.admin = result
      else: self.reader = result; self.readerId = 77
      return result
   def external(self, command, operation):
      if operation == 'client version': return self.state.get('versionOverride', (self.snapshot / 'client-version.txt').read_bytes())
      raise AssertionError('Unmocked external operation: ' + operation)
   def privacy(self, path, create=False):
      if self.platform == 'linux' and os.name != 'nt': return super().privacy(path, create)
      self.state['aclCalls'].append((str(path), create))
      if create:
         assert not path.exists()
         path.mkdir(mode=0o700)  # Isolated mock effect only, never the production Windows path.
      assert path.is_dir() and not path.is_symlink()
      if self.state.get('aclFail'): raise ValueError('Private ACL differs')
   def report(self, context, metadataBytes):
      self.save(context + '-server.tsv', metadataBytes)
      self.save(context + '-environment.json', '{"platform":"' + self.platform + '","note":"UTF-8 ☃"}\n')


def snapshot(directory, platform):
   host, version, case, dataset, _, package = R.PLATFORMS[platform]
   records = F.qaRecords(F.baseline())
   for rows in records.values():
      for row in rows:
         if row.get('id') == F.DATASET: row['id'] = dataset
         if row.get('datasetId') == F.DATASET: row['datasetId'] = dataset
   parts = []
   for table in R.INSPECT.evidence.TABLES:
      details = R.INSPECT.MANIFEST[table]
      parts.append(table + '\tCREATE TABLE `' + table.lower() + '` (' + ','.join('`' + field['name'] + '` ' + field['type'] for field in details['columns']) + ') ENGINE=InnoDB;\n')
   rawDdl = ''.join(parts).encode('utf-8')
   journal = records['SchemaMigration']
   journal[0].update(checksum=R.digest(ROOT / 'migrations/mariadb/001_PersistentTaskFoundation.sql'), schemaFingerprint='1' * 64)
   journal[1].update(checksum=R.digest(ROOT / 'migrations/mariadb/002_OrganizationalManagement.sql'), schemaFingerprint=R.ddlFingerprint(rawDdl))
   records['MigrationAttempt'][0].update(checksum=journal[1]['checksum'], baselineFingerprint=journal[0]['schemaFingerprint'],
                                       startedAt=1, completedAt=2, lastCompletedStep=9, attemptState='completed')
   newline = b'\r\n' if platform == 'windows' else b'\n'
   for name in R.CAPTURE_FILES: R.writeNew(directory / name, b'fixture\n')
   for name in ('attempt.txt', 'capture-complete.txt'):
      (directory / name).write_bytes(('reviewed-prefix ' + platform + ' taskcompass_dev B7.8 ' + package + '\n').encode('utf-8'))
   (directory / 'dump-exit.txt').write_bytes(b'0\n')
   (directory / 'client-version.txt').write_bytes(b'native fixture client 13.0.2' + newline)
   for moment in ('before', 'after'):
      (directory / ('source2-' + moment + '.hex.tsv')).write_bytes(F.export(records).encode('utf-8').replace(b'\n', newline))
      (directory / ('source2-' + moment + '.ddl.txt')).write_bytes(rawDdl.replace(b'\n', newline))
      healthSql = (ROOT / 'scripts/phase12/b7/schema2-health.sql').read_text(encoding='utf-8')
      rules = R.re.findall(r"^SELECT '([^']+)' AS ruleName,COUNT", healthSql, flags=R.re.M)
      (directory / ('health-' + moment + '.tsv')).write_bytes(('\n'.join(rule + '\t0' for rule in rules) + '\n').encode('utf-8').replace(b'\n', newline))
      (directory / ('grants-' + moment + '.tsv')).write_bytes(('SERVER\t' + '\t'.join([host, version, '3306', 'taskcompass_dev', 'root@localhost', case, '1', '1']) + '\n').encode('utf-8').replace(b'\n', newline))
   (directory / 'schema2.sql').write_bytes((D.dump(records) + 'COMMIT;\n').encode('utf-8').replace(b'\n', newline))
   report = 'environment-2026-11-21-fixture.json'
   (directory / 'environment-origin.txt').write_bytes((report + '\n').encode('utf-8'))
   R.writeNew(directory / report, '{"note":"☃"}\n')
   (directory / 'environment.json').write_bytes((directory / report).read_bytes())
   R.seal(directory, report + '.sha256', [report])
   R.writeNew(directory / 'private-review.txt', 'Owner explicitly attests private SQL review via --dump-reviewed.\n')
   files = R.CAPTURE_FILES | {report, report + '.sha256'}
   R.seal(directory, 'capture.sha256', files)
   R.seal(directory, 'schema2-evidence.sha256', files | {'capture.sha256', 'private-review.txt'})


class RestoreTests(unittest.TestCase):
   def setUp(self):
      self.temp = tempfile.TemporaryDirectory(prefix='B79 UTF8 ☃ spaces '); self.addCleanup(self.temp.cleanup)
      self.root = Path(self.temp.name)
      self.platforms = R.PLATFORMS.copy()
      self.addCleanup(setattr, R, 'PLATFORMS', self.platforms)
      self.mask = os.umask(0o077); self.addCleanup(os.umask, self.mask)
      self.states = {}
      for platform in ('linux', 'windows'):
         directory = self.root / platform; directory.mkdir(mode=0o700)
         snapshot(directory, platform)
         R.PLATFORMS[platform] = (*self.platforms[platform][:4], str(directory), self.platforms[platform][5])
         self.states[platform] = dict(policy='absent', populated=False, mutations=[], calls=[], aclCalls=[])
      self.addCleanup(patch.stopall)
      patch('getpass.getpass', return_value=PASSWORD).start()

   def runStage(self, platform, stage, directory=None):
      args = argparse.Namespace(stage=stage, platform=platform, run_dir=str(directory) if directory else None, client='fixture-only-client', build_dir=str(self.root / 'build with spaces'))
      runner = MockRestore(args, self.states[platform])
      try:
         with patch.dict(os.environ, {'MSYSTEM': 'UCRT64' if platform == 'windows' else '',
                                      'MSYS2_ENV_CONV_EXCL': 'MYSQL_PWD', 'LC_ALL': 'C', 'PYTHONUTF8': '1'}):
            with contextlib.redirect_stdout(io.StringIO()): runner.execute()
      finally: runner.close()
      return runner.directory

   def provision(self, platform='linux'):
      directory = self.runStage(platform, 'preflight')
      self.runStage(platform, 'provision', directory)
      return directory

   def testBothPlatformCompleteWorkflowAndPreservation(self):
      for platform in ('linux', 'windows'):
         with self.subTest(platform=platform):
            original = {p.name: p.read_bytes() for p in Path(R.PLATFORMS[platform][4]).iterdir()}
            directory = self.runStage(platform, 'preflight')
            for stage in R.STAGES[1:]: self.runStage(platform, stage, directory)
            self.assertEqual(self.states[platform]['mutations'], ['provision', 'restore', 'readonly'])
            self.assertEqual(self.states[platform]['policy'], 'readonly')
            self.assertEqual(original, {p.name: p.read_bytes() for p in Path(R.PLATFORMS[platform][4]).iterdir()})
            self.assertTrue((directory / 'readonly.sha256').exists())
            self.assertTrue(all(PASSWORD.encode('utf-8') not in p.read_bytes() for p in directory.iterdir()))
            with self.assertRaisesRegex(ValueError, 'Existing/partial'): self.runStage(platform, 'restore', directory)

   def testExistingTargetHostAndWrongServerFailBeforeProvision(self):
      for mode in ('existing', 'wrongServer'):
         state = self.states['linux']; state['policy'] = 'import' if mode == 'existing' else 'absent'; state['wrongServer'] = mode == 'wrongServer'
         with self.assertRaises(ValueError): self.runStage('linux', 'preflight')
         self.assertEqual(state['mutations'], [])
      state['wrongServer'] = False; state['policy'] = 'absent'
      directory = self.provision(); state['hostVariant'] = True
      with self.assertRaises(ValueError): self.runStage('linux', 'restore', directory)
      self.assertEqual(state['mutations'], ['provision'])

   def testSnapshotHashOrPackageMismatchNoConnection(self):
      directory = Path(R.PLATFORMS['linux'][4]); (directory / 'schema2.sql').write_bytes(b'changed')
      with self.assertRaisesRegex(ValueError, 'Checksum'): self.runStage('linux', 'preflight')
      self.assertEqual(self.states['linux']['calls'], [])
      directory = Path(R.PLATFORMS['windows'][4]); (directory / 'capture-complete.txt').write_bytes(b'other')
      with self.assertRaises(ValueError): self.runStage('windows', 'preflight')
      self.assertEqual(self.states['windows']['calls'], [])

   def testUnexpectedAuthorityOrObjectsAndPublicFailClosed(self):
      directory = self.provision('windows'); state = self.states['windows']
      for flag in ('badGrant', 'extraAuthority', 'crossSchema', 'storedCode', 'sessions', 'aclFail'):
         state[flag] = True
         with self.assertRaises(ValueError): self.runStage('windows', 'restore', directory)
         state[flag] = False
      state['public'] = b'GRANT INSERT ON *.* TO PUBLIC\n'
      with self.assertRaisesRegex(ValueError, 'PUBLIC'): self.runStage('windows', 'restore', directory)
      self.assertEqual(state['mutations'], ['provision'])

   def testPartialImportAndInterruptionNeverRetry(self):
      for platform, flag in [('linux', 'importFail'), ('windows', 'interrupt')]:
         directory = self.provision(platform); self.states[platform][flag] = True
         with self.assertRaises(ValueError): self.runStage(platform, 'restore', directory)
         self.assertTrue((directory / 'restore-attempt.txt').exists())
         self.assertFalse((directory / 'restore-success.txt').exists())
         with self.assertRaisesRegex(ValueError, 'Existing/partial'): self.runStage(platform, 'restore', directory)
         self.assertEqual(self.states[platform]['mutations'], ['provision', 'restore'])

   def testComparisonFailuresRetainReportsAndBlockReduction(self):
      for platform, flag in [('linux', 'dataMismatch'), ('windows', 'ddlMismatch')]:
         directory = self.provision(platform); self.runStage(platform, 'restore', directory)
         self.states[platform][flag] = True
         with self.assertRaises(ValueError): self.runStage(platform, 'verify', directory)
         self.assertTrue((directory / 'verify-attempt.txt').exists())
         with self.assertRaises((ValueError, OSError)): self.runStage(platform, 'readonly', directory)
         self.assertEqual(self.states[platform]['mutations'], ['provision', 'restore'])

   def testBadHealthExtraObjectAndColumnInventory(self):
      directory = self.provision(); self.runStage('linux', 'restore', directory)
      self.states['linux']['extraObject'] = True
      with self.assertRaises(ValueError): self.runStage('linux', 'verify', directory)
      self.states['linux']['extraObject'] = False; self.states['linux']['missingColumn'] = True
      with self.assertRaises(ValueError): self.runStage('linux', 'verify', directory)
      self.states['linux']['missingColumn'] = False; self.states['linux']['badHealth'] = True
      with self.assertRaises(ValueError): self.runStage('linux', 'verify', directory)
      self.assertTrue((directory / 'verify-health.tsv').exists())

   def testProvisionAndReadonlyFailuresPreserveAttempts(self):
      directory = self.runStage('linux', 'preflight')
      self.states['linux']['provisionFail'] = True
      with self.assertRaises(ValueError): self.runStage('linux', 'provision', directory)
      with self.assertRaisesRegex(ValueError, 'Existing/partial'): self.runStage('linux', 'provision', directory)
      self.assertEqual(self.states['linux']['mutations'], ['provision'])
      directory = self.provision('windows')
      self.runStage('windows', 'restore', directory); self.runStage('windows', 'verify', directory)
      self.states['windows']['dataMismatch'] = True
      with self.assertRaises(ValueError): self.runStage('windows', 'readonly', directory)
      self.assertTrue((directory / 'readonly-attempt.txt').exists())
      self.assertEqual(self.states['windows']['mutations'], ['provision', 'restore'])

   def testSnapshotProvenanceAndJournalRejectEvenWithNewLocalHashes(self):
      directory = Path(R.PLATFORMS['windows'][4])
      for name in ('attempt.txt', 'capture-complete.txt'):
         raw = (directory / name).read_bytes().replace(R.PLATFORMS['windows'][5].encode('ascii'), b'0' * 64)
         (directory / name).write_bytes(raw)
      (directory / 'capture.sha256').unlink(); (directory / 'schema2-evidence.sha256').unlink()
      report = (directory / 'environment-origin.txt').read_text(encoding='utf-8').strip()
      files = R.CAPTURE_FILES | {report, report + '.sha256'}
      R.seal(directory, 'capture.sha256', files)
      R.seal(directory, 'schema2-evidence.sha256', files | {'capture.sha256', 'private-review.txt'})
      with self.assertRaisesRegex(ValueError, 'provenance'): self.runStage('windows', 'preflight')
      self.assertEqual(self.states['windows']['calls'], [])
      raw = (Path(R.PLATFORMS['linux'][4]) / 'source2-before.hex.tsv')
      rows = R.INSPECT.evidence.readExport(raw, R.INSPECT.MANIFEST)
      rows = copy.deepcopy(rows)
      rows['SchemaMigration'][2][1]['checksum'] = '0' * 64
      with self.assertRaises(ValueError): R.checkJournal(rows, (raw.parent / 'source2-before.ddl.txt').read_bytes())

   def testNativeCommandIdentityAndNoReconnect(self):
      args = argparse.Namespace(stage='restore', platform='windows', run_dir=None, client='fake client with spaces', build_dir=None)
      runner = R.Restore(args)
      class LoginSession:
         def __init__(self, command, password):
            self.command = command; self.password = password
         def query(self, sql, operation):
            if operation == 'administrator session mode': return b''
            return ('\t'.join([runner.host, runner.version, '3306', R.TARGET, R.TARGET + '@127.0.0.1', '1', '1', '1', '77']) + '\r\n').encode('utf-8')
         def close(self): pass
      with patch.object(R, 'Client', LoginSession):
         session = runner.connect(admin=False)
         self.assertIn('--skip-reconnect', session.command)
         self.assertIn('--local-infile=0', session.command)
         self.assertIn('--database=' + R.TARGET, session.command)
         self.assertTrue(all(PASSWORD not in arg for arg in session.command))
         admin = runner.connect(admin=True)
         self.assertFalse(any(arg.startswith('--database') for arg in admin.command))
      runner.close()
      runner.directory = self.root / 'report path spaces'; runner.directory.mkdir()
      with patch.object(runner, 'external', return_value=b'') as external:
         runner.report('fixture', b'SERVER\tfixture\n')
      command = external.call_args[0][0]
      self.assertEqual(command[0], 'bash')
      self.assertTrue(all('\\' not in argument for argument in command))

   def testManifestTraversalDuplicatesAndTampering(self):
      directory = self.root / 'manifest'; directory.mkdir()
      for text in ('a' * 64 + '  ../outside\n', 'a' * 64 + '  item\n' + 'a' * 64 + '  item\n'):
         (directory / 'test.sha256').write_bytes(text.encode('utf-8'))
         with self.assertRaises(ValueError): R.checkManifest(directory, 'test.sha256')
      directory = self.provision(); (directory / 'binding.json').write_bytes(b'changed')
      with self.assertRaises(ValueError): self.runStage('linux', 'restore', directory)

   def testWrapperAndApprovalCannotConnect(self):
      env = os.environ.copy(); env['MSYSTEM'] = 'UCRT64'
      for platform in ('linux', 'windows'):
         for stage in ('01-preflight', '02-provision', '03-restore', '04-verify', '05-readonly'):
            path = str(ROOT / 'scripts/phase12/b79' / (stage + '.sh'))
            result = subprocess.run(['bash', path, '--platform', platform, '--print-plan'], env=env, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0)
      mismatch = subprocess.run(['bash', str(ROOT / 'scripts/phase12/b79/01-preflight.sh'), '--approved', '--platform', 'linux'], env=env, capture_output=True, check=False)
      self.assertNotEqual(mismatch.returncode, 0)
      self.assertIn(b'explicit platform', mismatch.stderr)
      result = subprocess.run([sys.executable, str(ROOT / 'scripts/phase12/b79/restore.py'), 'restore', '--platform', 'linux'], capture_output=True, check=False)
      self.assertNotEqual(result.returncode, 0)
      self.assertIn(b'--approved', result.stderr)

   def testWindowsPrivateCreationNeverUsesPythonMkdir(self):
      args = argparse.Namespace(stage='preflight', platform='windows', run_dir=None, client='fixture-only-client', build_dir=None)
      runner = R.Restore(args)
      path = self.root / 'new native output'
      with patch.object(runner, 'external', return_value=b'') as external, patch.object(Path, 'mkdir', side_effect=AssertionError('Python mkdir must not run')):
         runner.privacy(path, create=True)
      self.assertEqual(external.call_args[0][0][-2:], ['-Operation', 'new'])
      path.mkdir()
      with self.assertRaisesRegex(ValueError, 'already exists'): runner.privacy(path, create=True)

   def testAclHelperDiagnosticsAreStructuredAndRedacted(self):
      args = argparse.Namespace(stage='preflight', platform='windows', run_dir=None, client='fixture-only-client', build_dir=None)
      runner = R.Restore(args)
      stderr = ('untrusted ' + PASSWORD + '\nB79_ACL_ERROR type=System.Exception hresult=-1 detail=WIN32_183\n').encode('utf-8')
      with patch.object(R.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, b'', stderr)):
         with self.assertRaises(ValueError) as error: runner.external(['fake'], 'private ACL validation')
      self.assertIn('WIN32_183', str(error.exception))
      self.assertNotIn(PASSWORD, str(error.exception))
      self.assertNotIn('untrusted', str(error.exception))

   @unittest.skipUnless(os.name == 'nt', 'Native Windows privacy creation only')
   def testNativeWindowsPythonPrivacyCreation(self):
      args = argparse.Namespace(stage='preflight', platform='windows', run_dir=None, client='fixture-only-client', build_dir=None)
      runner = R.Restore(args)
      path = self.root / 'native output ☃ with spaces'
      runner.privacy(path, create=True)
      R.writeNew(path / 'fixture.txt', 'UTF8 ☃\n')
      runner.privacy(path)
      with self.assertRaisesRegex(ValueError, 'already exists'): runner.privacy(path, create=True)

   def testWindowsClientBannerSlashStyleOnly(self):
      path = 'C:/Program Files/MariaDB 13.0/bin/mariadb.exe'
      banner = ' from 13.0.2-MariaDB, client 15.2 for Win64 (AMD64), source revision 620a4b5624222048fece7fe1789978ae0675cae2'
      current = path + banner + '\r\n'
      captured = path.replace('/', '\\') + banner + '\n'
      self.assertTrue(R.sameClientVersion(current, captured, 'windows'))
      self.assertFalse(R.sameClientVersion(current, captured, 'linux'))
      for changed in (current.replace('13.0.2-MariaDB', '13.0.3-MariaDB'),
                      current.replace('client 15.2', 'client 15.3'),
                      current.replace('AMD64', 'ARM64'), current.replace('620a4b', '720a4b'),
                      current.replace('C:/', 'D:/'), current.replace('/bin/', '/other/'),
                      current.replace('mariadb.exe', 'mysql.exe'), current + 'extra',
                      './mariadb.exe' + banner):
         self.assertFalse(R.sameClientVersion(changed, captured, 'windows'))
      self.states['windows']['versionOverride'] = b'unexpected version\n'
      with self.assertRaisesRegex(ValueError, 'Native client version differs'):
         self.runStage('windows', 'preflight')
      self.assertEqual(self.states['windows']['calls'], [])

   def testClientSessionProtocolAndErrors(self):
      fake = self.root / 'fake client.py'
      fake.write_text("import sys\nfor line in sys.stdin.buffer:\n if line.startswith(b\"SELECT '__B79_\"):\n  sys.stdout.buffer.write(b'UTF-8 \\xe2\\x98\\x83\\r\\n' + line.split(b\"'\")[1] + b'\\r\\n'); sys.stdout.buffer.flush()\n", encoding='utf-8')
      client = R.Client([sys.executable, str(fake)])
      try: self.assertEqual(client.query('SELECT 1;', 'fixture'), 'UTF-8 ☃\r\n'.encode('utf-8'))
      finally: client.close()
      fake.write_text("import sys\nfor line in sys.stdin.buffer:\n if line.startswith(b\"SELECT '__B79_\"):\n  sys.stdout.buffer.write(line.split(b\"'\")[1] + b'\\n'); sys.stdout.buffer.flush(); sys.exit(7)\n", encoding='utf-8')
      client = R.Client([sys.executable, str(fake)])
      try:
         self.assertEqual(client.query('SELECT 1;', 'fixture'), b'')
         with self.assertRaisesRegex(ValueError, 'client exit'): client.finish('fixture completion')
      finally: client.close()
      fake.write_text("import sys\nsys.stderr.write('ERROR 1045 (28000): SECRET SQL\\n');sys.stderr.flush()\nsys.exit(1)\n", encoding='utf-8')
      client = R.Client([sys.executable, str(fake)])
      try:
         with self.assertRaises(ValueError) as error: client.query('SELECT 1;', 'fixture failure')
         self.assertNotIn('SECRET', str(error.exception))
      finally: client.close()


if __name__ == '__main__': unittest.main()
