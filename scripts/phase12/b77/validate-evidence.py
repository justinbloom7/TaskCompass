#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline comparison of sealed B7.5 HEX exports; no database/network access."""
import argparse
import csv
import json
import re
from pathlib import Path

TABLES = ('SchemaMigration', 'Dataset', 'Task', 'CommandRecord', 'LocalChange',
          'MigrationAttempt', 'Pillar', 'Project', 'Objective')
DOMAIN_TABLES = ('Task', 'Pillar', 'Project', 'Objective')
UUID_PATTERN = re.compile(r'^[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$')


def require(condition, message):
   if not condition:
      raise ValueError(message)


def decoded(token):
   if token == 'N':
      return None
   if re.fullmatch(r'I-?[0-9]+', token):
      return int(token[1:])
   if re.fullmatch(r'H(?:[0-9A-F]{2})*', token):
      return bytes.fromhex(token[1:]).decode('utf-8')
   raise ValueError('Invalid HEX-export value encoding')


def readExport(path, manifest):
   result = {name: {} for name in TABLES}
   counts = {}
   columns = {name: [column['name'] for column in manifest[name]['columns']] for name in TABLES}
   for line in Path(path).read_text(encoding='utf-8').splitlines():
      fields = line.split('\t')
      name = fields[0]
      if name.startswith('COUNT:'):
         name = name[6:]
         require(name in result and name not in counts and len(fields) == 2
                 and fields[1].isdigit(), 'Unknown/duplicate/malformed table count')
         counts[name] = int(fields[1])
         continue
      require(name in counts and len(fields) == len(columns[name]) + 1,
              'Unknown table or unexpected export column count')
      values = dict(zip(columns[name], map(decoded, fields[1:])))
      key = values.get('id', values.get('version'))
      require(key is not None and key not in result[name], 'Missing/duplicate record identity')
      if 'id' in values:
         require(isinstance(key, str) and UUID_PATTERN.fullmatch(key), 'Invalid stable UUID')
      result[name][key] = (tuple(fields[1:]), values)
   require(set(counts) == set(TABLES), 'Missing export table/count')
   for name in TABLES:
      require(counts[name] == len(result[name]), 'Export row count does not match contents')
   return result


def validate(baseline, current, prefix, dataset):
   require(UUID_PATTERN.fullmatch(dataset), 'Invalid expected dataset')
   require(prefix.startswith('B77-QA-') and '\n' not in prefix and '\t' not in prefix,
           'Invalid QA run prefix')
   for name in TABLES:
      for key, (tokens, _) in baseline[name].items():
         require(key in current[name] and current[name][key][0] == tokens,
                 'Protected record changed/missing: ' + name + '/' + str(key))
   for name in ('Dataset', 'SchemaMigration', 'MigrationAttempt'):
      require(current[name] == baseline[name], 'Immutable metadata changed: ' + name)
   require(set(current['Dataset']) == {dataset}, 'Dataset identity/count differs')
   new = {name: {key: values for key, (_, values) in current[name].items()
                 if key not in baseline[name]} for name in TABLES}
   for name in TABLES:
      for _, values in current[name].values():
         if 'datasetId' in values:
            require(values['datasetId'] == dataset, 'Cross-dataset record')
   for name in DOMAIN_TABLES:
      for key, values in new[name].items():
         title = values['name' if name == 'Pillar' else 'title']
         require(isinstance(title, str) and title.startswith(prefix + ' '),
                 'New record lacks this run QA prefix: ' + name + '/' + key)
   parents = {}
   for name in ('Pillar', 'Project', 'Objective'):
      for key, (_, values) in current[name].items():
         options = [('Pillar', values.get('parentPillarId'))] if name == 'Pillar' else [
            ('Pillar', values.get('pillarId')),
            ('Project', values.get('parentProjectId' if name == 'Project' else 'projectId'))]
         options = [pair for pair in options if pair[1] is not None]
         require(len(options) == (0 if name == 'Pillar' and not options else 1),
                 'Contradictory organizational parent')
         parent = options[0] if options else None
         parents[(name, key)] = parent
         if parent:
            require(parent[1] in current[parent[0]], 'Missing organizational parent')
            parentValues = current[parent[0]][parent[1]][1]
            require(values['deletedAt'] is not None or parentValues['deletedAt'] is None,
                    'Live entity has a removed parent')
            if key in new[name]:
               require(parent[1] in new[parent[0]], 'QA record attached to protected organization')
   # Iterative traversal; no arbitrary product depth cap.
   resolved = set()
   for key in parents:
      chain = set()
      cursor = key
      while cursor is not None and cursor not in resolved:
         require(cursor not in chain, 'Organizational hierarchy cycle')
         chain.add(cursor)
         cursor = parents[cursor]
      resolved.update(chain)
   for key, (_, values) in current['Task'].items():
      owners = [(name, values[column]) for name, column in (
         ('Objective', 'objectiveId'), ('Project', 'directProjectId')) if values[column] is not None]
      require(len(owners) <= 1, 'Task has contradictory organizational owners')
      for name, owner in owners:
         require(owner in current[name], 'Task owner missing')
         require(values['deletedAt'] is not None or current[name][owner][1]['deletedAt'] is None,
                 'Live task owner removed')
         if key in new['Task']:
            require(owner in new[name], 'QA task attached to protected organization')
   def target(values):
      targets = [(name, values[column]) for name, column in (
         ('Task', 'taskId'), ('Pillar', 'pillarId'), ('Project', 'projectId'),
         ('Objective', 'objectiveId')) if values[column] is not None]
      require(len(targets) == 1, 'Audit/outbox target is contradictory')
      name, key = targets[0]
      require(key in new[name], 'New audit/outbox targets protected or missing record')
      return targets[0]
   commands = {}
   for key, values in new['CommandRecord'].items():
      kind, entityId = target(values)
      require(isinstance(values['operationId'], str) and UUID_PATTERN.fullmatch(values['operationId']),
              'Invalid command operation UUID')
      require(values['payloadVersion'] == 2, 'New audit payload version differs')
      require(values['operationId'] not in commands, 'Duplicate command operation')
      commands[values['operationId']] = values
      after = json.loads(values['afterSnapshot'])
      require(isinstance(after, dict) and after.get('id') == entityId
              and after.get('entityType') == kind.lower() and after.get('datasetId') == dataset
              and str(after.get('localRevision')) == str(values['resultingRevision'])
              and str(after.get('updatedAt')) == str(values['recordedAt']),
              'Committed snapshot identity/revision/timestamp differs from audit')
      if values['beforeSnapshot'] is not None:
         before = json.loads(values['beforeSnapshot'])
         require(isinstance(before, dict) and before.get('id') == entityId
                 and before.get('entityType') == kind.lower() and before.get('datasetId') == dataset
                 and str(before.get('localRevision')) == str(values['resultingRevision'] - 1),
                 'Before snapshot identity/revision differs from audit')
      for reference in ('compensatesCommandId', 'reappliesCommandId'):
         require(values[reference] is None or values[reference] in new['CommandRecord'],
                 'QA inverse references protected/missing command')
   changes = set()
   for key, values in new['LocalChange'].items():
      owner = target(values)
      operation = values['operationId']
      require(operation in commands and operation not in changes, 'Outbox/command operation mismatch')
      changes.add(operation)
      command = commands[operation]
      require(owner == target(command) and values['payloadVersion'] == 2
              and values['localRevision'] == command['resultingRevision']
              and values['createdAt'] == command['recordedAt']
              and values['immutablePayload'] == command['afterSnapshot']
              and values['acknowledgmentState'] == 'pending', 'Outbox differs from committed command')
   require(changes == set(commands), 'Committed command lacks matching outbox')
   return new, parents


def writeInventory(path, current, new, parents):
   with Path(path).open('x', encoding='utf-8', newline='') as stream:
      writer = csv.writer(stream, delimiter='\t', lineterminator='\n')
      writer.writerow(('kind', 'id', 'title', 'parentKind', 'parentId', 'explicitArchive',
                       'effectiveArchive', 'deletedAt', 'status', 'revision'))
      for name in DOMAIN_TABLES:
         for key, values in sorted(new[name].items()):
            if name == 'Task':
               parent = next(((kind, values[column]) for kind, column in (
                  ('Objective', 'objectiveId'), ('Project', 'directProjectId')) if values[column]), None)
            else:
               parent = parents[(name, key)]
            archived = values.get('archivedAt') is not None
            cursor = parent
            while cursor:
               archived = archived or current[cursor[0]][cursor[1]][1]['archivedAt'] is not None
               cursor = parents[cursor]
            writer.writerow((name, key, values.get('name', values.get('title')),
                             parent[0] if parent else '', parent[1] if parent else '',
                             values.get('archivedAt'), archived, values['deletedAt'],
                             values.get('lifecycleStatus', ''), values['localRevision']))


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('--baseline', required=True)
   parser.add_argument('--current', required=True)
   parser.add_argument('--prefix', required=True)
   parser.add_argument('--dataset', required=True)
   parser.add_argument('--manifest', required=True)
   parser.add_argument('--inventory', required=True)
   parser.add_argument('--protected-inventory')
   args = parser.parse_args()
   try:
      manifest = json.loads(Path(args.manifest).read_text(encoding='utf-8'))['2']
      baseline = readExport(args.baseline, manifest)
      current = readExport(args.current, manifest)
      new, parents = validate(baseline, current, args.prefix, args.dataset)
      writeInventory(args.inventory, current, new, parents)
      if args.protected_inventory:
         protected = {name: {key: values for key, (_, values) in baseline[name].items()} for name in DOMAIN_TABLES}
         writeInventory(args.protected_inventory, current, protected, parents)
      print('PASS: every protected B7.5 row unchanged; new QA ownership/graph/audit/outbox consistent.')
   except (ValueError, KeyError, OSError, TypeError, UnicodeError) as error:
      # Our plain validation errors contain only invariant names/table/UUID, never values.
      detail = str(error) if type(error) is ValueError else 'Malformed evidence or inaccessible private file'
      print('STOP: ' + detail + '; preserve files for review.')
      return 1
   return 0


if __name__ == '__main__':
   raise SystemExit(main())
