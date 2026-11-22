#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""B7.9 operator orchestration. No live use without separate stage authorization."""
import argparse
import contextlib
import datetime
import getpass
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import queue
import re
import signal
import stat
import subprocess
import sys
import threading
import uuid

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
TARGET = 'taskcompass_phase12_restore'
STAGES = ('preflight', 'provision', 'restore', 'verify', 'readonly')
SPEC = importlib.util.spec_from_file_location('snapshotInspector79', ROOT / 'scripts/phase12/b78/inspect-snapshot.py')
INSPECT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INSPECT)
require = INSPECT.evidence.require
PLATFORMS = {
   'linux': ('feledria', '10.5.29-MariaDB', '0', '21be9da7-b767-4578-b972-cc9a35122ddb',
             '/home/justin/temp/TaskCompassBackups/Phase1.2-schema2-linux-20261120-204710-103433',
             '46dbffef69d1d85eabbc9982a190dbff81466190a075810ab259200d3a4ed4b5'),
   'windows': ('feldria-w10', '13.0.2-MariaDB', '1', '457bb52b-fea4-4347-9c47-675c6b035c90',
               'C:/Users/justin/TaskCompassBackups/Phase1.2-schema2-windows-20261121-222115-177176',
               'f0203488fd1b019f8a9475d0c4ac41d940700550c658d3723831cf7e225291ad')}
CAPTURE_FILES = set(('dump-exit.txt environment-origin.txt prior-evidence.txt attempt.txt schema2.sql '
   'source2-before.hex.tsv source2-after.hex.tsv source2-before.ddl.txt source2-after.ddl.txt '
   'health-before.tsv health-after.tsv grants-before.tsv grants-after.tsv preservation.txt '
   'dump-validation.txt environment.json client-version.txt dump-version.txt capture-complete.txt').split())


def digest(path):
   return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def sameClientVersion(current, captured, platform):
   current = current.replace('\r\n', '\n').strip()
   captured = captured.replace('\r\n', '\n').strip()
   if current == captured: return True
   if platform != 'windows': return False
   currentPath, separator, currentVersion = current.partition(' from ')
   capturedPath, capturedSeparator, capturedVersion = captured.partition(' from ')
   if not separator or not capturedSeparator or currentVersion != capturedVersion: return False
   # Only the absolute executable prefix may vary in slash style, not version data.
   pattern = r'[A-Za-z]:[\\/][^\r\n]*[\\/]mariadb\.exe'
   return (re.fullmatch(pattern, currentPath) is not None
           and re.fullmatch(pattern, capturedPath) is not None
           and currentPath.replace('\\', '/') == capturedPath.replace('\\', '/'))


def writeNew(path, data):
   if isinstance(data, str): data = data.encode('utf-8')
   with Path(path).open('xb') as output: output.write(data)


def regular(path):
   require(path.is_file() and not path.is_symlink(), 'Missing/linked evidence: ' + path.name)


def checkManifest(directory, name, expected=None):
   regular(directory / name)
   entries = {}
   for line in (directory / name).read_text(encoding='utf-8').splitlines():
      match = re.fullmatch(r'([0-9a-f]{64}) [ *]([A-Za-z0-9._-]+)', line)
      require(match is not None and match[2] not in entries and match[2] not in ('.', '..'), 'Invalid/duplicate manifest entry')
      entries[match[2]] = match[1]
   require(bool(entries) and (expected is None or set(entries) == set(expected)), 'Manifest coverage differs: ' + name)
   for filename, checksum in entries.items():
      regular(directory / filename)
      require(digest(directory / filename) == checksum, 'Checksum differs: ' + filename)
   return entries


def seal(directory, name, names):
   writeNew(directory / name, ''.join(digest(directory / item) + '  ' + item + '\n' for item in sorted(names)))


class Client:
   """One identity-checked session, no reconnect; stderr retained in memory only."""
   def __init__(self, command, password=None):
      env = os.environ.copy()
      for key in list(env):
         if key.startswith('MYSQL_') or key == 'MARIADB_PWD': env.pop(key)
      if password is not None: env['MYSQL_PWD'] = password
      self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, env=env)
      self.lines = queue.Queue()
      self.errors = bytearray()
      def readOutput():
         for line in iter(self.process.stdout.readline, b''): self.lines.put(line)
         self.lines.put(None)
      def readErrors():
         for line in iter(self.process.stderr.readline, b''):
            # Never retain echoed SQL, task values or passwords in evidence/logs.
            match = re.search(rb'ERROR (\d+)(?: \(([A-Z0-9]+)\))?', line)
            if match: self.errors.extend(match[0] + b'\n')
      threading.Thread(target=readOutput, daemon=True).start()
      self.errorThread = threading.Thread(target=readErrors, daemon=True)
      self.errorThread.start()

   def query(self, sql, operation):
      marker = ('__B79_' + uuid.uuid4().hex + '__').encode('ascii')
      if isinstance(sql, str): sql = sql.encode('utf-8')
      try:
         self.process.stdin.write(sql + b"\nSELECT '" + marker + b"';\n")
         self.process.stdin.flush()
         output = bytearray()
         while True:
            line = self.lines.get(timeout=60)
            if line is None:
               self.errorThread.join(timeout=1)
               raise ValueError(operation + ': client exited before completion ' + self.errors.decode('ascii'))
            if line.rstrip(b'\r\n') == marker: return bytes(output)
            output.extend(line)
      except (BrokenPipeError, queue.Empty):
         raise ValueError(operation + ': connection lost/timeout; outcome uncertain') from None

   def finish(self, operation):
      self.close()
      require(self.process.returncode == 0, operation + ': nonzero/uncertain client exit')

   def close(self):
      if self.process.poll() is None:
         try: self.process.stdin.close()
         except BrokenPipeError: pass
         try: self.process.wait(timeout=5)
         except subprocess.TimeoutExpired: self.process.kill(); self.process.wait()
      if not self.process.stdin.closed:
         try: self.process.stdin.close()
         except BrokenPipeError: pass
      self.process.stdout.close(); self.process.stderr.close()


def ddlFingerprint(raw):
   # Match MigrationRunner::fingerprint, retaining all DDL except table-name case.
   text = raw.decode('utf-8').replace('\r\n', '\n')
   matches = list(re.finditer(r'^([^\t\n]+)\t(CREATE TABLE .*?)(?=^[^\t\n]+\tCREATE TABLE |\Z)', text, re.M | re.S))
   tables = {}
   for match in matches:
      name = match[1].lower()
      require(name not in tables, 'Duplicate raw DDL table')
      tables[name] = match[2].rstrip('\n')
   require(set(tables) == {name.lower() for name in INSPECT.evidence.TABLES}, 'Raw DDL table coverage differs')
   result = ''
   for name in INSPECT.evidence.TABLES:
      ddl = tables[name.lower()]
      for table in INSPECT.evidence.TABLES:
         ddl = re.sub('`' + table + '`', '`' + table.lower() + '`', ddl, flags=re.I)
      result += ddl + '\n'
   return hashlib.sha256(result.encode('utf-8')).hexdigest()


def checkJournal(rows, ddl):
   journal = {key: entry[1] for key, entry in rows['SchemaMigration'].items()}
   require(set(journal) == {1, 2}, 'Expected exactly migrations 001 and 002')
   for version, filename in [(1, '001_PersistentTaskFoundation.sql'), (2, '002_OrganizationalManagement.sql')]:
      path = ROOT / 'migrations/mariadb' / filename
      require(re.fullmatch(r'[0-9a-f]{64}', journal[version]['schemaFingerprint']) is not None, 'Invalid recorded fingerprint')
      require(journal[version]['checksum'] == digest(path) and journal[version]['appliedAt'] > 0, 'Migration journal checksum/timestamp differs')
   require(journal[2]['schemaFingerprint'] == ddlFingerprint(ddl), 'Journal/raw DDL fingerprint differs')
   require(len(rows['MigrationAttempt']) == 1, 'Expected exactly one migration attempt')
   attempt = next(iter(rows['MigrationAttempt'].values()))[1]
   require(attempt['version'] == 2 and attempt['checksum'] == journal[2]['checksum']
           and attempt['baselineFingerprint'] == journal[1]['schemaFingerprint']
           and attempt['startedAt'] > 0 and attempt['completedAt'] is not None
           and attempt['completedAt'] >= attempt['startedAt'] and attempt['lastCompletedStep'] == 9
           and attempt['attemptState'] == 'completed', 'Incomplete/inconsistent migration attempt')


class Restore:
   def __init__(self, args):
      self.args = args
      self.platform = args.platform
      self.host, self.version, self.case, self.dataset, snapshot, self.originPackage = PLATFORMS[self.platform]
      self.snapshot = Path(snapshot)
      self.directory = Path(args.run_dir) if args.run_dir else None
      self.client = args.client or ('/usr/bin/mariadb' if self.platform == 'linux' else 'C:/Program Files/MariaDB 13.0/bin/mariadb.exe')
      self.build = Path(args.build_dir or ROOT / ('build' if self.platform == 'linux' else 'build-ucrt64'))
      self.admin = None; self.reader = None
      self.readerId = 0
      self.currentOperation = 'initialization'
      self.password = None

   def external(self, command, operation):
      result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
      require(result.returncode == 0, operation + ': helper failed (exit ' + str(result.returncode) + ')')
      return result.stdout

   def privacy(self, path, create=False):
      require(path.is_dir() and not path.is_symlink(), 'Missing/linked private directory')
      if self.platform == 'linux':
         info = path.stat()
         require(info.st_uid == os.getuid() and stat.S_IMODE(info.st_mode) == 0o700, 'Private directory owner/mode differs')
         for entry in path.iterdir():
            require(entry.is_file() and not entry.is_symlink() and entry.stat().st_uid == os.getuid()
                    and stat.S_IMODE(entry.stat().st_mode) == 0o600, 'Private file owner/mode differs')
      else:
         self.external(['powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File',
                        str(ROOT / 'scripts/phase12/b79/private-evidence.ps1'), '-Path', str(path),
                        '-Operation', 'create' if create else 'check'], 'private ACL validation')

   def validateSnapshot(self):
      self.currentOperation = 'sealed snapshot verification'
      require(ROOT.resolve() not in self.snapshot.resolve().parents, 'Private snapshot/evidence must be outside repository')
      self.privacy(self.snapshot)
      reportName = (self.snapshot / 'environment-origin.txt').read_text(encoding='utf-8').strip()
      require(re.fullmatch(r'environment-[A-Za-z0-9._-]+\.json', reportName) is not None, 'Invalid environment origin')
      files = CAPTURE_FILES | {reportName, reportName + '.sha256'}
      checkManifest(self.snapshot, 'capture.sha256', files)
      checkManifest(self.snapshot, 'schema2-evidence.sha256', files | {'capture.sha256', 'private-review.txt'})
      checkManifest(self.snapshot, reportName + '.sha256', {reportName})
      require((self.snapshot / 'environment.json').read_bytes() == (self.snapshot / reportName).read_bytes(), 'Environment copy differs')
      attempt = (self.snapshot / 'attempt.txt').read_bytes()
      require(attempt == (self.snapshot / 'capture-complete.txt').read_bytes(), 'Capture receipt mismatch')
      require(attempt.decode('utf-8').strip().endswith(' ' + self.platform + ' taskcompass_dev B7.8 ' + self.originPackage), 'Unapproved snapshot package/platform provenance')
      require((self.snapshot / 'private-review.txt').read_text(encoding='utf-8').startswith('Owner explicitly attests private SQL review via --dump-reviewed.\n'), 'Missing reviewed snapshot attestation')
      require((self.snapshot / 'dump-exit.txt').read_text(encoding='utf-8').strip() == '0', 'Dump exit is not zero')
      for suffix in ('hex.tsv', 'ddl.txt'):
         require((self.snapshot / ('source2-before.' + suffix)).read_bytes() == (self.snapshot / ('source2-after.' + suffix)).read_bytes(), 'Snapshot before/after differs')
      data = self.snapshot / 'source2-before.hex.tsv'
      rows = INSPECT.evidence.readExport(data, INSPECT.MANIFEST)
      require(set(rows['Dataset']) == {self.dataset}, 'Snapshot dataset differs')
      checkJournal(rows, (self.snapshot / 'source2-before.ddl.txt').read_bytes())
      with contextlib.redirect_stdout(io.StringIO()): INSPECT.checkDump(self.snapshot / 'schema2.sql', rows)
      for name in ('health-before.tsv', 'health-after.tsv'): self.health((self.snapshot / name).read_bytes())
      expectedServer = ['SERVER', self.host, self.version, '3306', 'taskcompass_dev', 'root@localhost', self.case, '1', '1']
      for name in ('grants-before.tsv', 'grants-after.tsv'):
         servers = [line.split('\t') for line in (self.snapshot / name).read_text(encoding='utf-8').splitlines() if line.startswith('SERVER\t')]
         require(servers == [expectedServer], 'Snapshot server provenance differs')
      require((self.snapshot / 'grants-before.tsv').read_bytes() == (self.snapshot / 'grants-after.tsv').read_bytes(), 'Snapshot grants before/after differ')
      require(sum(len(value['columns']) for value in INSPECT.MANIFEST.values()) == 85, 'Unexpected schema manifest version/column total')
      self.snapshotHash = digest(self.snapshot / 'schema2.sql')
      self.sealHash = digest(self.snapshot / 'schema2-evidence.sha256')
      self.packageHash = digest(HERE / 'ReviewChecksums.sha256')
      return rows

   def health(self, raw):
      source = (ROOT / 'scripts/phase12/b7/schema2-health.sql').read_text(encoding='utf-8')
      expected = re.findall(r"^SELECT '([^']+)' AS ruleName,COUNT", source, flags=re.M)
      actual = [line.split('\t') for line in raw.decode('utf-8').splitlines()]
      require(sorted(actual) == sorted([[name, '0'] for name in expected]), 'Missing/extra/nonzero health rules')

   def connect(self, admin=True):
      command = [self.client, '--no-defaults', '--skip-reconnect', '--connect-timeout=5',
                 '--default-character-set=utf8mb4', '--local-infile=0', '--batch', '--raw', '--skip-column-names', '--unbuffered', '--binary-mode']
      password = None
      if admin and self.platform == 'linux':
         # Keep sudo's own terminal password prompt visible; never capture it.
         require(subprocess.run(['sudo', '-v'], check=False).returncode == 0, 'sudo authentication failed')
         command = ['sudo', '-n', '--'] + command + ['--protocol=SOCKET', '--user=root']
      else:
         command += ['--protocol=TCP', '--host=127.0.0.1', '--port=3306', '--user=' + ('root' if admin else TARGET)]
         if admin: password = getpass.getpass('[windows] MariaDB root password: ')
         else:
            if self.password is None: self.password = getpass.getpass('Restore account password: ')
            password = self.password
            command += ['--database=' + TARGET]
      connection = Client(command, password)
      if admin:
         self.admin = connection
         # Explicit session rendering mode, no global setting or selected database.
         self.admin.query("SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';", 'administrator session mode')
      else:
         self.reader = connection
         values = connection.query('SELECT @@hostname,VERSION(),@@port,DATABASE(),CURRENT_USER(),@@lower_case_table_names,@@foreign_key_checks,@@check_constraint_checks,CONNECTION_ID();', 'restore login identity').decode('utf-8').strip().split('\t')
         require(values[:-1] == [self.host, self.version, '3306', TARGET, TARGET + '@127.0.0.1', self.case, '1', '1'], 'Restore login identity differs')
         require(values[-1].isdigit(), 'Invalid connection identity')
         self.readerId = int(values[-1])
      return connection

   def preflight(self, policy, populated=False):
      raw = self.admin.query('SET @b79_reader_id=' + str(self.readerId) + ';\n' + (HERE / 'preflight.sql').read_text(encoding='utf-8'), 'administrator metadata ' + policy)
      grouped = {}
      for line in raw.decode('utf-8').splitlines():
         fields = line.split('\t')
         require(fields[1:] not in grouped.get(fields[0], []), 'Duplicate metadata row')
         grouped.setdefault(fields[0], []).append(fields[1:])
      require(set(grouped) <= {'SERVER', 'DATABASE', 'ACCOUNT', 'GLOBAL', 'RIGHT', 'EXTRA', 'OBJECT', 'COLUMN', 'INVALID'}, 'Unknown metadata tag')
      require(grouped.get('SERVER') == [[self.host, self.version, '3306', 'NULL', 'root@localhost', self.case, '1', '1']], 'Wrong administrator/server identity')
      require(grouped.get('EXTRA') == [['0'] * 5] and grouped.get('INVALID') == [['0'] * 5], 'Extra authority/objects/cross-schema references/sessions')
      if self.platform == 'windows':
         public = self.admin.query('SHOW GRANTS FOR PUBLIC;', 'PUBLIC privilege check').decode('utf-8').splitlines()
         require(bool(public), 'Missing PUBLIC privilege report')
         for line in public:
            require(re.fullmatch(r'GRANT [A-Z, ]+ ON `(?:test|test\\_%)`\.\* TO PUBLIC', line) is not None
                    or line == 'GRANT USAGE ON *.* TO PUBLIC', 'Unexpected PUBLIC privilege/role scope')
         raw += ('\n'.join('PUBLIC\t' + line for line in public) + '\n').encode('utf-8')
      if policy == 'absent':
         require(not any(grouped.get(tag) for tag in ('DATABASE', 'ACCOUNT', 'GLOBAL', 'RIGHT', 'OBJECT', 'COLUMN')), 'Restore target/account already exists; no reuse')
      else:
         require(grouped.get('DATABASE') == [[TARGET, 'utf8mb4', 'utf8mb4_unicode_ci']], 'Restore encoding/database differs')
         require(grouped.get('ACCOUNT') == [[TARGET, '127.0.0.1', 'N']], 'Restore account host/role differs')
         require(grouped.get('GLOBAL', []) in ([], [['USAGE', 'NO']]), 'Global privileges differ')
         rights = ['SELECT'] if policy == 'readonly' else ['SELECT', 'INSERT', 'CREATE']
         require(sorted(grouped.get('RIGHT', [])) == sorted([[TARGET.replace('_', '\\_'), right, 'NO'] for right in rights]), 'Exact restore grants differ')
         expectedObjects = sorted([[name.lower(), 'BASE TABLE', 'InnoDB'] for name in INSPECT.evidence.TABLES]) if populated else []
         expectedColumns = sorted([[table.lower(), field['name']] for table, details in INSPECT.MANIFEST.items() for field in details['columns']]) if populated else []
         require(sorted(grouped.get('OBJECT', [])) == expectedObjects and sorted(grouped.get('COLUMN', [])) == expectedColumns, 'Restore table/85-column inventory differs')
      return raw

   def binding(self):
      return json.dumps({'platform': self.platform, 'target': TARGET, 'snapshot': str(self.snapshot),
                         'dumpSha256': self.snapshotHash, 'sealSha256': self.sealHash,
                         'originPackageSha256': self.originPackage, 'operatorPackageSha256': self.packageHash}, sort_keys=True) + '\n'

   def save(self, name, data):
      writeNew(self.directory / name, data)

   def report(self, context, metadata):
      name = context + '-server.tsv'
      self.save(name, metadata)
      self.external(['bash', (ROOT / 'scripts/environment-report.sh').as_posix(), '--build-dir', self.build.as_posix(),
                     '--context', 'B7.9-' + context, '--server-evidence', (self.directory / name).as_posix(),
                     '--snapshot-dir', self.directory.as_posix()], 'environment report')

   def verifyData(self, prefix):
      for sql, filename in [('schema2-export.sql', 'data.hex.tsv'), ('ddl2-export.sql', 'ddl.txt'), ('schema2-health.sql', 'health.tsv')]:
         raw = self.reader.query((ROOT / 'scripts/phase12/b7' / sql).read_bytes(), 'restore ' + sql)
         self.save(prefix + '-' + filename, raw)
         if sql == 'schema2-health.sql': self.health(raw)
         else:
            expected = 'source2-before.' + ('hex.tsv' if sql == 'schema2-export.sql' else 'ddl.txt')
            require(raw == (self.snapshot / expected).read_bytes(), 'Restored bytes differ: ' + filename)
      rows = INSPECT.evidence.readExport(self.directory / (prefix + '-data.hex.tsv'), INSPECT.MANIFEST)
      checkJournal(rows, (self.directory / (prefix + '-ddl.txt')).read_bytes())
      self.save(prefix + '-comparison.txt', 'PASS: all nine tables/85 columns, exact data/history/journals/DDL, UUID, fingerprint and health.\n')

   def execute(self):
      os.umask(0o077)
      self.validateSnapshot()
      print('Sealed snapshot: ' + str(self.snapshot), flush=True)
      print('Approved-file candidate schema2.sql SHA256: ' + self.snapshotHash, flush=True)
      print('Original B7.8 package SHA256: ' + self.originPackage, flush=True)
      self.currentOperation = 'native client version'
      version = self.external([self.client, '--no-defaults', '--version'], 'client version')
      require(sameClientVersion(version.decode('utf-8'), (self.snapshot / 'client-version.txt').read_text(encoding='utf-8'), self.platform), 'Native client version differs from sealed capture')
      stage = self.args.stage
      if stage == 'preflight':
         require(self.directory is None, 'Stage01 always creates a fresh evidence directory')
         self.connect()
         metadata = self.preflight('absent')
         self.directory = self.snapshot.parent / ('Phase1.2-restore-' + self.platform + '-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:12])
         self.directory.mkdir(mode=0o700)
         self.privacy(self.directory, create=True)
         print('Private B7.9 evidence directory: ' + str(self.directory), flush=True)
         self.save('binding.json', self.binding())
         self.save('client-version.txt', version)
      else:
         require(self.directory is not None and self.directory.parent.resolve() == self.snapshot.parent.resolve()
                 and self.directory.name.startswith('Phase1.2-restore-' + self.platform + '-'), 'Wrong restore evidence directory')
         self.privacy(self.directory)
         require((self.directory / 'binding.json').read_text(encoding='utf-8') == self.binding(), 'Restore evidence/snapshot/package binding differs')
         index = STAGES.index(stage)
         for prior in STAGES[:index]: checkManifest(self.directory, prior + '.sha256')
         for later in STAGES[index:]:
            require(not any((self.directory / (later + suffix)).exists() for suffix in ('-attempt.txt', '-success.txt', '.sha256')), 'Existing/partial operation: no retry/resume')
         self.connect()
         if stage == 'provision': metadata = self.preflight('absent')
         else:
            self.connect(admin=False)
            metadata = self.preflight('import', populated=stage in ('verify', 'readonly'))
      self.currentOperation = stage
      self.save(stage + '-before.tsv', metadata)
      self.save(stage + '-attempt.txt', self.binding())
      if stage == 'provision':
         self.password = getpass.getpass('New restore-account password (12+ characters): ')
         require(len(self.password) >= 12 and not any(char in self.password for char in '\r\n\0'), 'Invalid new password length/control character')
         template = (ROOT / 'scripts/phase12/b7/restore-account.template.sql').read_text(encoding='utf-8')
         require(template.count('{{PASSWORD_SQL_LITERAL}}') == 1, 'Provision template token differs')
         sql = template.replace('{{PASSWORD_SQL_LITERAL}}', "'" + self.password.replace("'", "''") + "'")
         self.admin.query(sql, 'one-shot restore provisioning')
         del sql
         self.connect(admin=False)
         self.save(stage + '-after.tsv', self.preflight('import'))
      elif stage == 'restore':
         # Recheck exact file digest immediately before the single import.
         require(digest(self.snapshot / 'schema2.sql') == self.snapshotHash, 'Dump changed before import')
         self.reader.query((self.snapshot / 'schema2.sql').read_bytes(), 'one-shot exact dump import')
         self.reader.finish('exact dump import completion'); self.reader = None; self.readerId = 0
         self.save('restore-exit.txt', '0\n')
         self.save(stage + '-after.tsv', self.preflight('import', populated=True))
      elif stage == 'verify':
         self.verifyData(stage)
         self.save(stage + '-after.tsv', self.preflight('import', populated=True))
      elif stage == 'readonly':
         # Repeat equality/health before changing authority; never repair drift.
         self.verifyData('readonly-before')
         self.admin.query((ROOT / 'scripts/phase12/b7/restore-account-readonly.sql').read_bytes(), 'one-shot revoke INSERT/CREATE')
         self.reader.finish('pre-reduction reader completion'); self.reader = None; self.readerId = 0
         self.save(stage + '-after.tsv', self.preflight('readonly', populated=True))
         self.connect(admin=False)
         self.verifyData('readonly-after')
      for label in ('reader', 'admin'):
         connection = getattr(self, label)
         if connection is not None:
            connection.finish(stage + ' ' + label + ' completion')
            setattr(self, label, None)
            self.save(stage + '-' + label + '-exit.txt', '0\n')
      after = self.directory / (stage + '-after.tsv')
      self.report(stage, after.read_bytes() if after.exists() else metadata)
      self.save(stage + '-success.txt', self.binding())
      self.privacy(self.directory)
      names = [entry.name for entry in self.directory.iterdir() if entry.name != stage + '.sha256']
      seal(self.directory, stage + '.sha256', names)
      print('PASS: B7.9 ' + stage + '; evidence ' + str(self.directory))
      print('STOP: next stage/platform requires separate approval; no cleanup.')

   def close(self):
      if self.reader: self.reader.close()
      if self.admin: self.admin.close()
      self.password = None


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('stage', choices=STAGES)
   parser.add_argument('--platform', required=True, choices=PLATFORMS)
   parser.add_argument('--approved', action='store_true')
   parser.add_argument('--run-dir')
   parser.add_argument('--client')
   parser.add_argument('--build-dir')
   args = parser.parse_args()
   if not args.approved:
      parser.error('separate stage/platform --approved authorization required')
   runner = Restore(args)
   def interrupted(signum, frame): raise ValueError('Interrupted; outcome uncertain')
   signal.signal(signal.SIGINT, interrupted); signal.signal(signal.SIGTERM, interrupted)
   try:
      expected = 'windows' if os.name == 'nt' else 'linux'
      require(args.platform == expected, 'Native Python platform differs; use Linux Bash or Windows UCRT64/native Python')
      os.umask(0o077)
      # Pin current code/SQL only, not historical B7.8 package equality.
      for line in (HERE / 'ReviewChecksums.sha256').read_text(encoding='utf-8').splitlines():
         checksum, filename = line.split('  ', 1)
         require(not Path(filename).is_absolute() and '..' not in Path(filename).parts
                 and digest(ROOT / filename) == checksum, 'Operator source checksum differs')
      runner.execute(); return 0
   except (ValueError, OSError, UnicodeError, KeyError, IndexError, TypeError) as error:
      # Never expose arbitrary exceptions which could contain SQL/private data.
      detail = str(error) if type(error) is ValueError else type(error).__name__
      print('STOP: ' + runner.currentOperation + ': ' + detail + '. Preserve state/artifacts; no retry/reset/repair/cleanup.', file=sys.stderr)
      return 1
   finally: runner.close()


if __name__ == '__main__': sys.exit(main())
