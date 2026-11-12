#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Documentation generator for migration 001; never connects to a database.

Uses Python's standard library and an installed Mermaid CLI/browser. This is
an intentionally narrow DDL reader, not a SQL parser or migration interpreter.
Extend/review it when later migrations change the schema.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

DIRECTORY = Path(__file__).resolve().parent
REPOSITORY = DIRECTORY.parent.parent
MIGRATION = REPOSITORY / 'migrations/mariadb/001_PersistentTaskFoundation.sql'
CONFIG = DIRECTORY / 'MermaidConfig.json'
TABLE_ORDER = ['Dataset', 'Task', 'CommandRecord', 'LocalChange', 'SchemaMigration']
LOGICAL_LINKS = [('compensatesCommandId', 'compensation'), ('reappliesCommandId', 'reapplication')]


def parseMigration(sql):
   tables = {}
   for match in re.finditer(r'CREATE TABLE (\w+) \((.*?)\) ENGINE=([^;]+);', sql, re.S):
      name, body, options = match.groups()
      table = {'columns': [], 'foreignKeys': [], 'uniqueKeys': [], 'checks': [], 'indexes': [], 'options': options}
      for line in body.strip().splitlines():
         line = line.strip().removesuffix(',')
         foreignKey = re.fullmatch(r'CONSTRAINT (\w+) FOREIGN KEY \(([^)]+)\) REFERENCES (\w+)\(([^)]+)\)', line)
         check = re.fullmatch(r'CONSTRAINT (\w+) CHECK \((.*)\)', line)
         uniqueKey = re.fullmatch(r'UNIQUE KEY (\w+) \(([^)]+)\)', line)
         index = re.fullmatch(r'INDEX (\w+) \(([^)]+)\)', line)
         column = re.fullmatch(r'(\w+) (INT|BIGINT|MEDIUMTEXT|CHAR\(\d+\)|VARCHAR\(\d+\)) (.+)', line)
         if foreignKey:
            keyName, columns, parent, parentColumns = foreignKey.groups()
            table['foreignKeys'].append((keyName, columns.split(', '), parent, parentColumns.split(', ')))
         elif check:
            table['checks'].append(check.groups())
         elif uniqueKey:
            table['uniqueKeys'].append((uniqueKey[1], uniqueKey[2].split(', ')))
         elif index:
            table['indexes'].append((index[1], index[2].split(', ')))
         elif column:
            columnName, dataType, modifiers = column.groups()
            nullable = not ('NOT NULL' in modifiers or 'PRIMARY KEY' in modifiers)
            keys = []
            if 'PRIMARY KEY' in modifiers:
               keys.append('PK')
            if 'UNIQUE' in modifiers:
               table['uniqueKeys'].append((columnName, [columnName]))
            table['columns'].append({'name': columnName, 'type': dataType, 'nullable': nullable,
                                     'keys': keys, 'modifiers': modifiers})
         else:
            raise ValueError('Unrecognized DDL; review generator before updating: ' + line)
      tables[name] = table
   if set(tables) != set(TABLE_ORDER) or len(tables) != 5:
      raise ValueError('Expected migration 001 table set; review cumulative migrations/generator.')
   residue = re.sub(r'CREATE TABLE \w+ \(.*?\) ENGINE=[^;]+;', '', sql, flags=re.S)
   if residue.strip():
      raise ValueError('Unsupported SQL outside CREATE TABLE statements.')
   for name, table in tables.items():
      for column in table['columns']:
         if any(column['name'] in key[1] for key in table['foreignKeys']):
            column['keys'].append('FK')
         if any(column['name'] in key[1] for key in table['uniqueKeys']):
            column['keys'].append('UK')
      for keyName, columns, parent, parentColumns in table['foreignKeys']:
         assert parent in tables and len(columns) == len(parentColumns), keyName
         assert set(columns) <= {column['name'] for column in table['columns']}, keyName
         assert set(parentColumns) <= {column['name'] for column in tables[parent]['columns']}, keyName
   assert {column for column, description in LOGICAL_LINKS} <= {column['name'] for column in tables['CommandRecord']['columns']}
   return tables


def columnComment(column):
   comment = 'NULL' if column['nullable'] else 'NOT NULL'
   if 'DEFAULT' in column['modifiers']:
      comment += "; default 'pending'"
   if column['name'] in ['compensatesCommandId', 'reappliesCommandId']:
      comment += '; logical reference only - no FK'
   if 'UK' in column['keys'] and column['name'] in ['datasetId', 'id']:
      comment += '; composite UK taskDatasetIdentity'
   return comment


def mermaidDiagram(tables):
   lines = ['erDiagram', '   direction LR']
   for name in TABLE_ORDER:
      lines.append('   ' + name + ' {')
      for column in tables[name]['columns']:
         keys = ' ' + ','.join(column['keys']) if column['keys'] else ''
         lines.append('      ' + column['type'] + ' ' + column['name'] + keys + ' "' + columnComment(column) + '"')
      lines.append('   }')
   for name in TABLE_ORDER:
      for keyName, columns, parent, parentColumns in tables[name]['foreignKeys']:
         childCardinality = 'o|' if any(key[1] == columns for key in tables[name]['uniqueKeys']) else 'o{'
         lines.append('   ' + parent + ' ||--' + childCardinality + ' ' + name + ' : "FK ' + keyName + '"')
   for column, description in LOGICAL_LINKS:
      lines.append('   CommandRecord o|..o{ CommandRecord : "LOGICAL ' + column + ' - no FK"')
   return '\n'.join(lines)


def markdown(tables, digest):
   text = '''<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# TaskCompass database schema — Phase 1.1 migration 001

Authority: [001_PersistentTaskFoundation.sql](../../migrations/mariadb/001_PersistentTaskFoundation.sql),
not the preliminary future model. This is the schema defined by migration source;
it is **not evidence of a deployed database or successful live migration**.
Only these five tables exist in this migration. No future pillars, projects,
objectives, contexts, recurrence or planning tables are included.

[Open the standalone SVG](DatabaseSchema.svg) in Firefox; zoom for individual fields.

## Mermaid ER diagram

Legend: **solid lines labeled FK** are actual SQL foreign keys; **dashed lines
labeled LOGICAL/no FK** are intended command-history references with no referential
constraint. The line styles are a documentation convention; the explicit FK labels
and constraint inventory below are authoritative. Each comment explicitly states
NULL/NOT NULL. PK = primary key; FK = foreign-key member; UK = unique-key member.
Task.datasetId and Task.id share **one composite UK**, not two individual uniques.
Parent participation is optional: a parent need not have children.

```mermaid
'''
   text += mermaidDiagram(tables) + '\n```\n\n'
   text += '''## Primary and unique constraints

| Table | Primary key | Additional unique constraints |
| --- | --- | --- |
'''
   for name in TABLE_ORDER:
      table = tables[name]
      primary = ', '.join(column['name'] for column in table['columns'] if 'PK' in column['keys'])
      uniqueDescriptions = []
      for keyName, columns in table['uniqueKeys']:
         inline = len(columns) == 1 and any(column['name'] == columns[0] and 'UNIQUE' in column['modifiers'] for column in table['columns'])
         uniqueDescriptions.append('`UNIQUE(' + columns[0] + ')` (inline)' if inline else '`' + keyName + '(' + ', '.join(columns) + ')`')
      unique = '; '.join(uniqueDescriptions) or 'None'
      text += '| ' + name + ' | `' + primary + '` | ' + unique + ' |\n'
   text += '''
Inline UNIQUE(operationId) is shown without inventing a constraint name; the migration does not
explicitly name those two UNIQUE constraints. PKs themselves are also unique.

## Enforced SQL relationships

| Constraint | Child columns | Referenced columns | SQL cardinality |
| --- | --- | --- | --- |
'''
   for name in TABLE_ORDER:
      for keyName, columns, parent, parentColumns in tables[name]['foreignKeys']:
         cardinality = 'Each child has exactly one parent; parent has zero or one child' if any(key[1] == columns for key in tables[name]['uniqueKeys']) else 'Each child has exactly one parent; parent has zero or many children'
         text += '| `' + keyName + '` | `' + name + '(' + ', '.join(columns) + ')` | `' + parent + '(' + ', '.join(parentColumns) + ')` | ' + cardinality + ' |\n'
   text += '''
All FK columns are NOT NULL. No explicit ON DELETE/ON UPDATE actions are declared;
there is no declared cascading delete/update. SchemaMigration is disconnected.
CommandRecord and LocalChange reach Dataset transitively through their composite
Task foreign keys; neither declares a separate datasetId → Dataset FK.

## Logical references and limits of enforcement

| Logical reference | Nullability | SQL enforcement |
| --- | --- | --- |
| CommandRecord.compensatesCommandId → CommandRecord.id | Nullable | No FK or uniqueness constraint; intended zero/one target per receipt, potentially many receipts per target. |
| CommandRecord.reappliesCommandId → CommandRecord.id | Nullable | No FK or uniqueness constraint; same intended cardinality. |

Dashed self-links describe intended meaning, **not guarantees** that the referenced
row exists, belongs to the same dataset, has a compatible command type or precedes
the referring command. No FK is declared on LocalChange.id. Its operationId, rather
than id, is the actual enforced reference to CommandRecord.

The separate changeTask and changeCommand constraints do **not** require a change
and its referenced command to name the same task/dataset; application transactions
coordinate that invariant. SQL permits a command with no LocalChange, even though
the application writes both atomically. JSON validity does not validate snapshot
shape or equality to Task, nor do the constraints enforce revision sequencing.
The singleton Dataset, schema version/checksum interpretation and chronological
history are application/migration-tool rules, not additional SQL relationships.

## CHECK constraints

These expressions are copied exactly from the migration; omitted application rules
are not presented as SQL constraints. UUID regex checks exist only on Dataset.id
and Task.id. Other CHAR(36) fields do not acquire regex checks implicitly.

| Table | Constraint | SQL expression |
| --- | --- | --- |
'''
   for name in TABLE_ORDER:
      for keyName, expression in tables[name]['checks']:
         text += '| ' + name + ' | `' + keyName + '` | `' + expression + '` |\n'
   text += '''
`taskLifecycle` enforces completedAt exactly when status is completed. Task timestamps
are checked against createdAt/updatedAt; positive creation time is explicitly checked
only for Dataset and Task. Not every metadata timestamp has a CHECK. JSON fields
remain MEDIUMTEXT with JSON_VALID checks; they are not physical JSON columns.

## Declared indexes and storage

| Table | Non-unique index | Ordered columns |
| --- | --- | --- |
'''
   for name in TABLE_ORDER:
      for keyName, columns in tables[name]['indexes']:
         text += '| ' + name + ' | `' + keyName + '` | `' + ', '.join(columns) + '` |\n'
   text += '''
Primary/unique indexes are enumerated above. The migration declares no other
non-unique indexes. InnoDB may create supporting indexes for foreign keys (notably
LocalChange.datasetId/taskId); engine-generated names are not asserted as explicit
migration declarations or verified live artifacts.

Every table uses InnoDB and default utf8mb4/utf8mb4_unicode_ci. Explicit CHAR identity,
checksum and fingerprint columns use ascii/ascii_bin; lifecycleStatus, commandType
and acknowledgmentState are also ascii/ascii_bin. Integer timestamps are BIGINT
UTC epoch milliseconds by application convention, not SQL DATETIME types.
LocalChange.acknowledgmentState has the sole explicit column default: 'pending'.
Every other column requires a value or permits NULL as shown; no AUTO_INCREMENT.

## Regeneration and validation

Generated directly from migration 001 by [GenerateDatabaseSchema.py](GenerateDatabaseSchema.py).
The helper reads repository schema/config files and invokes the local renderer;
it never connects to MariaDB. It is a narrow
reader for this migration's CREATE TABLE syntax, not a general migration interpreter.
When subsequent migrations change the cumulative schema, extend/review the helper
and update both artifacts against the resulting actual DDL. Do not substitute the
future logical model. Both diagrams share the same parsed table/column/key metadata.

```bash
python3 docs/diagrams/GenerateDatabaseSchema.py
python3 docs/diagrams/GenerateDatabaseSchema.py --check
```

Dependencies: Python 3.9+ standard library, Mermaid CLI (`mmdc`) and its browser.
Mermaid CLI 12.2.0 is now installed and has successfully rendered this ER source.
The SVG is a **native Mermaid export**, replacing the earlier Graphviz fallback.
[MermaidConfig.json](MermaidConfig.json) selects a landscape layout, neutral theme,
explicit font/spacing and deterministic IDs. Both logical self-references are
rendered separately. The helper adds provenance metadata and an 80-pixel outer
viewBox margin to prevent CLI 12.2.0 from clipping upper self-reference labels;
it does not change Mermaid's table/connector layout. No software was installed by
the agent.

`--check` does not launch a browser: it verifies SQL/Markdown agreement, embedded
SQL/Mermaid/config provenance, all SVG table fields/types/keys/nullability, four
solid FK links, two dashed logical links, landscape orientation and no external
resources or scripts. It does not promise byte-identical exports across renderer
versions. Review regenerated artifacts after tool upgrades.

The SVG uses XHTML `foreignObject` labels supported by Firefox/Chromium. The local
librsvg preview cannot display those labels; use a browser or a Mermaid-generated
PNG for visual review. Disabling HTML labels in CLI 12.2.0 produced clipped/wrapped
text in this ER diagram, so that configuration is deliberately not used. Browser
sandbox restrictions may require an approved unsandboxed tool invocation; do not
add Chromium `--no-sandbox` flags or install software merely to suppress errors.
Headless Firefox opened the final standalone SVG successfully; its screenshot was
visually inspected with all labels and separate logical connectors visible.
Interactive Firefox use has not been tested.

Mermaid source uses standard [ER syntax](https://mermaid.js.org/syntax/entityRelationshipDiagram.html),
with nullability comments rather than relying on recent nullable-type syntax.

Migration SHA-256: `'''
   return text + digest + '`.\n'


def provenance(tables):
   return {'migrationSha256': hashlib.sha256(MIGRATION.read_bytes()).hexdigest(),
           'mermaidSha256': hashlib.sha256(mermaidDiagram(tables).encode()).hexdigest(),
           'configSha256': hashlib.sha256(CONFIG.read_bytes()).hexdigest()}


def validateArtifacts(tables, svg):
   counts = [len(tables), sum(len(table['columns']) for table in tables.values()),
             sum('PK' in column['keys'] for table in tables.values() for column in table['columns']),
             sum(len(table['uniqueKeys']) for table in tables.values()),
             sum(len(table['foreignKeys']) for table in tables.values()),
             sum(len(table['checks']) for table in tables.values()),
             sum(len(table['indexes']) for table in tables.values())]
   assert counts == [5, 38, 5, 3, 4, 12, 3], counts
   root = ET.fromstring(svg)
   ns = {'s': 'http://www.w3.org/2000/svg'}
   metadata = root.find('s:metadata[@id="TaskCompassProvenance"]', ns)
   assert metadata is not None and json.loads(metadata.text) == provenance(tables)
   nodes = {}
   for node in root.findall('.//s:g', ns):
      if 'node' not in node.attrib.get('class', '').split():
         continue
      for name in tables:
         if '-entity-' + name + '-' in node.attrib.get('id', ''):
            nodes[name] = [text.strip() for text in node.itertext() if text.strip()]
   assert set(nodes) == set(tables)
   for name, table in tables.items():
      expected = [name]
      for column in table['columns']:
         expected.extend([column['type'], column['name']])
         if column['keys']:
            expected.append(','.join(column['keys']))
         expected.append(columnComment(column))
      assert nodes[name] == expected, (name, nodes[name])
   edges = [path for path in root.findall('.//s:path', ns) if 'relationshipLine' in path.attrib.get('class', '').split()]
   assert len(edges) == 6
   assert sum('edge-pattern-solid' in edge.attrib['class'] for edge in edges) == 4
   assert sum('edge-pattern-dashed' in edge.attrib['class'] for edge in edges) == 2
   assert not root.findall('.//s:script', ns) and not root.findall('.//s:image', ns)
   for element in root.iter():
      for key, value in element.attrib.items():
         if key.split('}')[-1] == 'href':
            assert value.startswith('#'), value
   assert '<!DOCTYPE' not in svg and '@import' not in svg
   for url in re.findall(r'url\([\'"]?([^\)\'"]+)', svg):
      assert url.startswith(('#', 'data:')), url
   viewBox = list(map(float, root.attrib['viewBox'].split()))
   assert viewBox[2] > viewBox[3]


def renderMermaid(tables):
   with tempfile.TemporaryDirectory(prefix='taskcompass-mermaid-') as temporary:
      output = Path(temporary) / 'schema.svg'
      command = ['mmdc', '-i', '-', '-o', str(output), '-c', str(CONFIG),
                 '-I', 'TaskCompassSchema', '-t', 'neutral', '-b', 'white']
      result = subprocess.run(command, input=mermaidDiagram(tables), text=True,
                              capture_output=True, check=True)
      svg = output.read_text()
      # CLI 12.2 may exclude upper self-reference labels from its viewBox.
      # Add a deterministic outer margin without changing Mermaid's layout.
      root = ET.fromstring(svg)
      bounds = list(map(float, root.attrib['viewBox'].split()))
      padding = 80
      bounds = [bounds[0] - padding, bounds[1] - padding, bounds[2] + 2 * padding, bounds[3] + 2 * padding]
      openingEnd = svg.index('>', svg.index('<svg'))
      opening = svg[:openingEnd]
      opening = re.sub(r'viewBox="[^"]+"', 'viewBox="' + ' '.join(map(str, bounds)) + '"', opening)
      opening = re.sub(r'width="[^"]+"', 'width="' + str(bounds[2]) + '"', opening, count=1)
      opening = re.sub(r'height="[^"]+"', 'height="' + str(bounds[3]) + '"', opening, count=1)
      svg = opening + svg[openingEnd:]
      # Keep Mermaid's native SVG/XHTML markup; add provenance without reserializing.
      metadata = '<metadata id="TaskCompassProvenance">' + json.dumps(provenance(tables), sort_keys=True) + '</metadata>'
      start = svg.index('>', svg.index('<svg')) + 1
      svg = svg[:start] + metadata + svg[start:]
      if result.stderr:
         print(result.stderr.strip())
      return svg


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('--check', action='store_true', help='Validate committed artifacts without launching a browser or writing files.')
   arguments = parser.parse_args()
   if list(MIGRATION.parent.glob('*.sql')) != [MIGRATION]:
      raise ValueError('Additional migrations require cumulative-schema generator review.')
   source = MIGRATION.read_bytes()
   tables = parseMigration(source.decode())
   digest = hashlib.sha256(source).hexdigest()
   expectedMarkdown = markdown(tables, digest)
   if arguments.check:
      assert (DIRECTORY / 'DatabaseSchema.md').read_text() == expectedMarkdown, 'Stale Markdown: regenerate and review.'
      svg = (DIRECTORY / 'DatabaseSchema.svg').read_text()
   else:
      svg = renderMermaid(tables)
   validateArtifacts(tables, svg)
   if not arguments.check:
      (DIRECTORY / 'DatabaseSchema.md').write_text(expectedMarkdown)
      (DIRECTORY / 'DatabaseSchema.svg').write_text(svg)
   print(('Verified' if arguments.check else 'Generated') + ' Mermaid Markdown and native SVG: 5 tables, 38 columns, 4 SQL FKs, 2 logical references.')


if __name__ == '__main__':
   main()
