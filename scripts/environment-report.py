#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline build/source inventory. Never connects to a database or prints secrets."""
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import uuid
from datetime import datetime, timezone
from pathlib import Path

CACHE_KEYS = ('CMAKE_CXX_COMPILER', 'CMAKE_BUILD_TYPE', 'CMAKE_GENERATOR',
              'Qt6Core_DIR', 'Qt6Widgets_DIR')
SOURCE_PATHS = ('src', 'scripts', 'tests', 'migrations', 'docs', 'CMakeLists.txt',
                'AGENTS.md', 'README.md', 'LICENSE', '.gitignore')


def command(arguments, directory):
   try:
      environment = {key: value for key, value in os.environ.items()
                     if not re.search(r'PASSWORD|SECRET|TOKEN|MYSQL_PWD', key, re.I)}
      result = subprocess.run(arguments, cwd=directory, env=environment, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, timeout=15, check=False)
      return result.stdout if result.returncode == 0 else b''
   except (OSError, subprocess.TimeoutExpired):
      return b''


def cacheValues(directory):
   result = {}
   path = directory / 'CMakeCache.txt'
   if path.is_file():
      for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
         match = re.match(r'([^:#]+):[^=]+=(.*)', line)
         if match and match[1] in CACHE_KEYS:
            result[match[1]] = match[2]
   return result


def qtVersion(directory, module):
   for suffix in ('ConfigVersionImpl.cmake', 'ConfigVersion.cmake'):
      path = Path(directory) / (module + suffix)
      if path.is_file():
         match = re.search(r'set\(PACKAGE_VERSION\s+"([0-9.]+)"', path.read_text(encoding='utf-8'))
         if match:
            return match[1]
   return 'unavailable'


def firstLine(arguments, directory):
   data = command(arguments, directory).decode('utf-8', errors='replace').splitlines()
   return data[0] if data else 'unavailable'


def sourceState(repository):
   head = firstLine(['git', 'rev-parse', 'HEAD'], repository)
   branch = firstLine(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], repository)
   status = command(['git', 'status', '--porcelain=v1', '-z', '--untracked-files=all'], repository)
   dirty = bool(status) if head != 'unavailable' else 'unavailable'
   digest = hashlib.sha256()
   names = command(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard',
                    '--', *SOURCE_PATHS], repository).split(b'\0')
   for name in sorted(set(names) - {b''}):
      relative = os.fsdecode(name)
      path = repository / relative
      digest.update(name + b'\0')
      if path.is_symlink():
         digest.update(os.fsencode(os.readlink(path)))
      elif path.is_file():
         digest.update(path.read_bytes())
      else:
         digest.update(b'<missing>')
      digest.update(b'\0')
   return {'git_commit': head, 'git_branch': branch, 'git_dirty': dirty,
           'source_sha256': digest.hexdigest() if head != 'unavailable' else 'unavailable'}


def collect(repository, build, serverEvidence=None):
   cache = cacheValues(build)
   operatingSystem = platform.platform()
   if platform.system() == 'Linux' and Path('/etc/os-release').is_file():
      for line in Path('/etc/os-release').read_text().splitlines():
         if line.startswith('PRETTY_NAME='):
            operatingSystem = line.split('=', 1)[1].strip('"') + ' / ' + platform.release()
   compiler = cache.get('CMAKE_CXX_COMPILER') or shutil.which('c++') or shutil.which('g++')
   fields = {'platform': operatingSystem, 'machine': platform.machine(),
             'msystem': os.environ.get('MSYSTEM', 'none'),
             'cmake': firstLine(['cmake', '--version'], repository),
             'compiler': firstLine([compiler, '--version'], repository) if compiler else 'unavailable',
             'build_directory': str(build), 'build_type': cache.get('CMAKE_BUILD_TYPE', 'unconfigured'),
             'generator': cache.get('CMAKE_GENERATOR', 'unconfigured'),
             'qt_core_package': qtVersion(cache['Qt6Core_DIR'], 'Qt6Core') if 'Qt6Core_DIR' in cache else 'unavailable',
             'qt_widgets_package': qtVersion(cache['Qt6Widgets_DIR'], 'Qt6Widgets') if 'Qt6Widgets_DIR' in cache else 'unavailable',
             'qt_version_provenance': 'CMake-selected installed package metadata; not a runtime DLL probe',
             'server': 'not queried (offline)', 'server_evidence': 'none'}
   if serverEvidence:
      lines = Path(serverEvidence).read_text(encoding='utf-8').splitlines()
      rows = [line.split('\t') for line in lines if line.startswith('SERVER\t')]
      if len(rows) != 1 or len(rows[0]) < 5:
         raise ValueError('Invalid saved server metadata')
      row = rows[0]
      fields['server'] = row[1] + ' / ' + row[2] + ' / TCP ' + row[3] + ' / ' + row[4]
      fields['server_evidence'] = 'saved verified metadata: ' + str(serverEvidence)
   binary = build / ('TaskCompass.exe' if (build / 'TaskCompass.exe').is_file() else 'TaskCompass')
   fields['desktop_binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest() if binary.is_file() else 'not built'
   fields.update(sourceState(repository))
   # Whitelisted tool output still must not reproduce any inherited secret value.
   secrets = [value for key, value in os.environ.items() if value and re.search(r'PASSWORD|SECRET|TOKEN|MYSQL_PWD', key, re.I)]
   for key, value in fields.items():
      if isinstance(value, str):
         for secret in secrets:
            value = value.replace(secret, '<redacted>')
         fields[key] = value
   return fields


def saveReport(fields, output, context, snapshotDirectory=None):
   output.mkdir(parents=True, exist_ok=True)
   previousFiles = sorted(output.glob('environment-*.json'))
   previous = json.loads(previousFiles[-1].read_text(encoding='utf-8'))['fields'] if previousFiles else {}
   changed = sorted(key for key in (set(previous) | set(fields)) - {'server_evidence'}
                    if previous.get(key) != fields.get(key))
   timestamp = datetime.now(timezone.utc).isoformat(timespec='microseconds')
   basename = 'environment-' + timestamp.replace(':', '').replace('+', '_') + '-' + uuid.uuid4().hex[:8]
   record = {'recorded_at_utc': timestamp, 'context': context, 'fields': fields, 'changed_fields': changed}
   payload = (json.dumps(record, ensure_ascii=False, indent=3) + '\n').encode('utf-8')
   snapshot = output / (basename + '.json')
   with snapshot.open('xb') as stream:
      stream.write(payload)
   if changed:
      block = '\n## ' + timestamp + ' — ' + context + '\n\n'
      for key in changed:
         block += '- ' + key + ': ' + str(previous.get(key, '<first report>')) + ' → ' + str(fields.get(key)) + '\n'
      descriptor = os.open(str(output / 'BuildNotes.md'), os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o600)
      try:
         os.write(descriptor, block.encode('utf-8'))
      finally:
         os.close(descriptor)
   if snapshotDirectory:
      destination = Path(snapshotDirectory) / snapshot.name
      with destination.open('xb') as stream:
         stream.write(payload)
      with destination.with_suffix('.json.sha256').open('x', encoding='utf-8') as stream:
         stream.write(hashlib.sha256(payload).hexdigest() + '  ' + destination.name + '\n')
   return snapshot, changed


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('--build-dir')
   parser.add_argument('--output-dir')
   parser.add_argument('--context', default='manual')
   parser.add_argument('--server-evidence')
   parser.add_argument('--snapshot-dir')
   args = parser.parse_args()
   os.umask(0o077)
   repository = Path(__file__).resolve().parents[1]
   build = Path(args.build_dir or repository / ('build-ucrt64' if os.environ.get('MSYSTEM') else 'build')).resolve()
   try:
      fields = collect(repository, build, args.server_evidence)
      snapshot, changed = saveReport(fields, Path(args.output_dir or build / 'validation-notes'), args.context, args.snapshot_dir)
      print('Platform: ' + fields['platform'] + '; ' + fields['msystem'])
      print('Qt Widgets package: ' + fields['qt_widgets_package'])
      print('MariaDB server: ' + fields['server'] + ' (' + fields['server_evidence'] + ')')
      print('Source: ' + fields['git_commit'] + '; dirty=' + str(fields['git_dirty']) + '; SHA256=' + fields['source_sha256'])
      print('Environment changes: ' + (', '.join(changed) if changed else 'none'))
      print('Build notes: ' + str(snapshot.parent / 'BuildNotes.md'))
      print('Environment snapshot: ' + str(snapshot))
   except (OSError, ValueError, KeyError):
      print('STOP: environment report failed; no database connection or mutation attempted.')
      return 1
   return 0


if __name__ == '__main__':
   raise SystemExit(main())
