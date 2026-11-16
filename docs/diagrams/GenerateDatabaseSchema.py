#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply the reviewed CREATE/ALTER subset of actual migrations 001+002 for docs.

No SQL engine, database access, shell SQL execution, or software installation.
Unknown migrations or syntax fail closed; this is not a general SQL interpreter.
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
MIGRATIONS = [REPOSITORY / 'migrations/mariadb/001_PersistentTaskFoundation.sql',
              REPOSITORY / 'migrations/mariadb/002_OrganizationalManagement.sql']
CONFIG = DIRECTORY / 'MermaidConfig.json'
TABLE_ORDER = ['Dataset', 'Pillar', 'Project', 'Objective', 'Task', 'CommandRecord', 'LocalChange', 'SchemaMigration', 'MigrationAttempt']
LOGICAL_LINKS = ['compensatesCommandId', 'reappliesCommandId']


def splitSql(text, delimiter):
   parts, start, depth, quoted, index = [], 0, 0, False, 0
   while index < len(text):
      character = text[index]
      if character == "'":
         if quoted and index + 1 < len(text) and text[index + 1] == "'":
            index += 2
            continue
         quoted = not quoted
      elif not quoted:
         if character == '(':
            depth += 1
         elif character == ')':
            depth -= 1
         elif character == delimiter and depth == 0:
            if text[start:index].strip():
               parts.append(text[start:index].strip())
            start = index + 1
         if depth < 0:
            raise ValueError('Unbalanced SQL')
      index += 1
   if quoted or depth:
      raise ValueError('Unbalanced SQL')
   if text[start:].strip():
      parts.append(text[start:].strip())
   return parts


def definition(table, source, modify=False):
   line = re.sub(r'\s+', ' ', source.strip())
   foreign = re.fullmatch(r'CONSTRAINT (\w+) FOREIGN KEY \(([^)]+)\) REFERENCES (\w+)\(([^)]+)\)', line)
   check = re.fullmatch(r'CONSTRAINT (\w+) CHECK \((.*)\)', line)
   unique = re.fullmatch(r'UNIQUE KEY (\w+) \(([^)]+)\)', line)
   index = re.fullmatch(r'INDEX (\w+) \(([^)]+)\)', line)
   column = re.fullmatch(r'(\w+) (INT|BIGINT|MEDIUMTEXT|CHAR\(\d+\)|VARCHAR\(\d+\)) (.+)', line)
   if foreign:
      table['foreignKeys'].append((foreign[1], [x.strip() for x in foreign[2].split(',')], foreign[3], [x.strip() for x in foreign[4].split(',')]))
   elif check:
      table['checks'].append((check[1], check[2]))
   elif unique:
      table['uniqueKeys'].append((unique[1], [x.strip() for x in unique[2].split(',')]))
   elif index:
      table['indexes'].append((index[1], [x.strip() for x in index[2].split(',')]))
   elif column:
      name, dataType, modifiers = column.groups()
      value = {'name': name, 'type': dataType, 'modifiers': modifiers,
               'nullable': 'NOT NULL' not in modifiers and 'PRIMARY KEY' not in modifiers, 'keys': []}
      existing = [i for i, item in enumerate(table['columns']) if item['name'] == name]
      if modify:
         if len(existing) != 1:
            raise ValueError('MODIFY missing column')
         table['columns'][existing[0]] = value
      else:
         if existing:
            raise ValueError('Duplicate column')
         table['columns'].append(value)
         if 'UNIQUE' in modifiers:
            table['uniqueKeys'].append((name, [name]))
   else:
      raise ValueError('Unsupported DDL definition: ' + line)


def parseMigrations(sources):
   tables = {}
   for source in sources:
      for statement in splitSql(source, ';'):
         created = re.fullmatch(r'CREATE TABLE (\w+) \((.*)\) ENGINE=(.*)', statement, re.S)
         altered = re.fullmatch(r'ALTER TABLE (\w+)\s+(.*)', statement, re.S)
         if created:
            name, body, options = created.groups()
            if name in tables:
               raise ValueError('Duplicate table')
            table = {'columns': [], 'foreignKeys': [], 'checks': [], 'uniqueKeys': [], 'indexes': [], 'options': options}
            for part in splitSql(body, ','):
               definition(table, part)
            tables[name] = table
         elif altered:
            name, body = altered.groups()
            table = tables[name]
            for part in splitSql(body, ','):
               dropped = re.fullmatch(r'DROP (FOREIGN KEY|CONSTRAINT) (\w+)', part)
               if dropped:
                  collection = 'foreignKeys' if dropped[1] == 'FOREIGN KEY' else 'checks'
                  old = table[collection]
                  table[collection] = [item for item in old if item[0] != dropped[2]]
                  if len(old) != len(table[collection]) + 1:
                     raise ValueError('Unknown dropped constraint')
               elif part.startswith('ADD COLUMN '):
                  definition(table, part[len('ADD COLUMN '):])
               elif part.startswith('MODIFY COLUMN '):
                  definition(table, part[len('MODIFY COLUMN '):], modify=True)
               elif part.startswith('ADD '):
                  definition(table, part[4:])
               else:
                  raise ValueError('Unsupported ALTER clause: ' + part)
         else:
            raise ValueError('Unsupported migration statement: ' + statement[:80])
   for table in tables.values():
      for column in table['columns']:
         column['keys'] = ['PK'] if 'PRIMARY KEY' in column['modifiers'] else []
         if any(column['name'] in key[1] for key in table['foreignKeys']):
            column['keys'].append('FK')
         if any(column['name'] in key[1] for key in table['uniqueKeys']):
            column['keys'].append('UK')
      for keyName, columns, parent, parentColumns in table['foreignKeys']:
         assert parent in tables and len(columns) == len(parentColumns), keyName
         assert set(columns) <= {column['name'] for column in table['columns']}, keyName
         assert set(parentColumns) <= {column['name'] for column in tables[parent]['columns']}, keyName
         assert any(key[1] == parentColumns for key in tables[parent]['uniqueKeys']) or parentColumns == [column['name'] for column in tables[parent]['columns'] if 'PK' in column['keys']], keyName
   return tables


def columnComment(column):
   text = 'NULL' if column['nullable'] else 'NOT NULL'
   default = re.search(r"DEFAULT ('[^']*'|NULL)", column['modifiers'])
   if default:
      text += '; default ' + default[1]
   if column['name'] in LOGICAL_LINKS:
      text += '; logical reference only - no FK'
   return text


def nullableForeign(table, columns):
   return any(column['nullable'] for column in table['columns'] if column['name'] in columns)


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
         parentCardinality = 'o|' if nullableForeign(tables[name], columns) else '||'
         childCardinality = 'o|' if any(key[1] == columns for key in tables[name]['uniqueKeys']) else 'o{'
         lines.append('   ' + parent + ' ' + parentCardinality + '--' + childCardinality + ' ' + name + ' : "FK ' + keyName + '"')
   for column in LOGICAL_LINKS:
      lines.append('   CommandRecord o|..o{ CommandRecord : "LOGICAL ' + column + ' - no FK"')
   return '\n'.join(lines)


def provenance(tables):
   return {'migrationSha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in MIGRATIONS},
           'mermaidSha256': hashlib.sha256(mermaidDiagram(tables).encode()).hexdigest(),
           'configSha256': hashlib.sha256(CONFIG.read_bytes()).hexdigest()}


def markdown(tables):
   text = '''<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# TaskCompass database schema — cumulative migrations 001–002

Authority: [001_PersistentTaskFoundation.sql](../../migrations/mariadb/001_PersistentTaskFoundation.sql)
and [002_OrganizationalManagement.sql](../../migrations/mariadb/002_OrganizationalManagement.sql).
This is the cumulative schema defined by actual migration source, **not evidence
that migration 002 has been executed or accepted on a live database**. Phase 1.1
schema deployment/evidence remains preserved in its milestone.

[Open the standalone SVG](DatabaseSchema.svg) in Firefox; zoom for individual fields.
Solid lines labeled FK represent actual SQL constraints; dashed LOGICAL links are
unenforced references. PK/UK/FK annotate column membership, not individual uniqueness
of composite-key members. NULL/NOT NULL and explicit defaults are included.
Optional FK lines reflect SQL nullability, not permission to violate exclusive
parent/target CHECK constraints. No cascading delete/update actions are declared.

## Mermaid ER diagram

```mermaid
'''
   text += mermaidDiagram(tables) + '\n```\n\n'
   text += '## Primary and unique keys\n\n| Table | Primary key | Additional unique keys |\n| --- | --- | --- |\n'
   for name in TABLE_ORDER:
      primary = ', '.join(column['name'] for column in tables[name]['columns'] if 'PK' in column['keys'])
      uniques = '; '.join('`' + key + '(' + ', '.join(columns) + ')`' for key, columns in tables[name]['uniqueKeys']) or 'None'
      text += '| ' + name + ' | `' + primary + '` | ' + uniques + ' |\n'
   text += '\nInline operationId UNIQUE constraints are shown using their column name; no explicit SQL constraint name is invented.\n'
   text += '\n## Enforced SQL foreign keys\n\n| Constraint | Child columns | Referenced columns | Child-to-parent participation |\n| --- | --- | --- | --- |\n'
   for name in TABLE_ORDER:
      for key, columns, parent, parentColumns in tables[name]['foreignKeys']:
         participation = 'Zero or one (nullable FK member)' if nullableForeign(tables[name], columns) else 'Exactly one'
         text += '| `' + key + '` | `' + name + '(' + ', '.join(columns) + ')` | `' + parent + '(' + ', '.join(parentColumns) + ')` | ' + participation + ' |\n'
   text += '''
A referenced parent may have zero children. Composite (datasetId, id) keys prevent
cross-dataset references. Project/Objective each have exactly one organizational
parent; Task has at most one objective or direct project; CommandRecord/LocalChange
each target exactly one of four entity types. Audit target columns identify the
mutated entity, not redundant task ownership. MigrationAttempt and SchemaMigration
are local metadata with no SQL FK between their version columns.

## Logical references and enforcement limits

CommandRecord.compensatesCommandId/reappliesCommandId are nullable logical references
to CommandRecord.id with no FK/uniqueness guarantee. Each may have many referring
receipts; SQL does not ensure existence, dataset, type or chronological validity.
LocalChange.id has no FK: operationId is its enforced command reference.
The separate target and command FKs do not ensure LocalChange and its command
name the same entity/dataset/revision. Application transactions enforce that and
atomic receipt/change insertion. SQL permits a command with no LocalChange.
Ancestor cycles, live-parent availability, inherited archive, singleton dataset,
revision ordering and snapshot shape/equality are application/migration rules.
JSON_VALID alone checks JSON syntax. Version-1 payloads remain task-only; new
version-2 payloads support four targets. Partial migrations require explicit review.

## CHECK constraints

Expressions below come from cumulative SQL (whitespace normalized), with removed
001 constraints omitted and replacements named explicitly.

| Table | Constraint | SQL expression |
| --- | --- | --- |
'''
   for name in TABLE_ORDER:
      for key, expression in tables[name]['checks']:
         text += '| ' + name + ' | `' + key + '` | `' + expression + '` |\n'
   text += '\n## Declared non-unique indexes\n\n| Table | Index | Ordered columns |\n| --- | --- | --- |\n'
   for name in TABLE_ORDER:
      for key, columns in tables[name]['indexes']:
         text += '| ' + name + ' | `' + key + '` | `' + ', '.join(columns) + '` |\n'
   text += '''
Primary/unique indexes are listed separately. InnoDB can add supporting FK indexes;
engine-generated index names are not asserted as explicit migration declarations.
All tables use InnoDB and default utf8mb4/utf8mb4_unicode_ci. Identity/checksum/
fingerprint and controlled status strings use explicit ascii/ascii_bin as in source.
BIGINT timestamps are UTC epoch milliseconds by application convention. No physical
SQL JSON columns, AUTO_INCREMENT, organization completion lifecycle, ordering,
contexts, planning or sync tables are introduced. archivedAt is explicit; inherited
archive is derived and never stored on descendants as redundant flags.

## Regeneration and validation

[GenerateDatabaseSchema.py](GenerateDatabaseSchema.py) interprets only the reviewed
CREATE/ALTER subset from the two exact migration files, and fails on unknown syntax
or an additional migration. Extend/review the reader for every later migration;
never substitute the preliminary logical model or assume all SQL is supported.
No database is accessed. Dependencies remain Python 3.9+ standard library and
installed Mermaid CLI/browser for rendering; --check needs no browser.

```bash
python3 docs/diagrams/GenerateDatabaseSchema.py
python3 docs/diagrams/GenerateDatabaseSchema.py --check
```

The SVG is a native Mermaid export with deterministic IDs/provenance and an outer
80-pixel viewBox margin to avoid clipping self-reference labels. It uses XHTML
foreignObject labels supported by Firefox/Chromium; librsvg does not render those
labels correctly. Do not disable browser sandboxing or install dependencies to
work around renderer restrictions without approval. A source schema change is not
live migration approval. Render/visual inspection results are in Phase1.2.md.

Migration SHA-256 values:\n\n'''
   for path in MIGRATIONS:
      text += '- `' + path.name + '`: `' + hashlib.sha256(path.read_bytes()).hexdigest() + '`\n'
   return text


def validateArtifacts(tables, svg):
   assert set(tables) == set(TABLE_ORDER)
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
      assert nodes[name] == expected, name
   edges = [path for path in root.findall('.//s:path', ns) if 'relationshipLine' in path.attrib.get('class', '').split()]
   expectedEdges = sum(len(table['foreignKeys']) for table in tables.values())
   assert sum('edge-pattern-solid' in edge.attrib['class'] for edge in edges) == expectedEdges
   assert sum('edge-pattern-dashed' in edge.attrib['class'] for edge in edges) == 2
   assert not root.findall('.//s:script', ns) and not root.findall('.//s:image', ns)
   for element in root.iter():
      for key, value in element.attrib.items():
         if key.split('}')[-1] == 'href':
            assert value.startswith('#'), value
   assert '<!DOCTYPE' not in svg and '@import' not in svg
   for url in re.findall(r'url\([\'"]?([^\)\'"]+)', svg):
      assert url.startswith(('#', 'data:')), url
   bounds = list(map(float, root.attrib['viewBox'].split()))
   assert bounds[2] > bounds[3]


def renderMermaid(tables):
   with tempfile.TemporaryDirectory(prefix='taskcompass-mermaid-') as temporary:
      output = Path(temporary) / 'schema.svg'
      result = subprocess.run(['mmdc', '-i', '-', '-o', str(output), '-c', str(CONFIG),
                               '-I', 'TaskCompassSchema', '-t', 'neutral', '-b', 'white'],
                              input=mermaidDiagram(tables), text=True, capture_output=True, check=True)
      svg = output.read_text()
      bounds = list(map(float, ET.fromstring(svg).attrib['viewBox'].split()))
      bounds = [bounds[0] - 80, bounds[1] - 80, bounds[2] + 160, bounds[3] + 160]
      end = svg.index('>', svg.index('<svg'))
      opening = re.sub(r'viewBox="[^"]+"', 'viewBox="' + ' '.join(map(str, bounds)) + '"', svg[:end])
      opening = re.sub(r'width="[^"]+"', 'width="' + str(bounds[2]) + '"', opening, count=1)
      opening = re.sub(r'height="[^"]+"', 'height="' + str(bounds[3]) + '"', opening, count=1)
      svg = opening + svg[end:]
      start = svg.index('>', svg.index('<svg')) + 1
      metadata = '<metadata id="TaskCompassProvenance">' + json.dumps(provenance(tables), sort_keys=True) + '</metadata>'
      svg = svg[:start] + metadata + svg[start:]
      if result.stderr:
         print(result.stderr.strip())
      return svg


def main():
   parser = argparse.ArgumentParser(description=__doc__)
   parser.add_argument('--check', action='store_true')
   parser.add_argument('--manifest-only', action='store_true', help='Refresh structural registry JSON without rendering or database access.')
   arguments = parser.parse_args()
   if set(MIGRATIONS[0].parent.glob('*.sql')) != set(MIGRATIONS):
      raise ValueError('Unknown migration; review cumulative reader.')
   sources = [path.read_text() for path in MIGRATIONS]
   tables = parseMigrations(sources)
   manifestPath = MIGRATIONS[0].parent / 'SchemaManifest.json'
   expectedManifest = json.dumps({str(version): parseMigrations(sources[:version]) for version in [1, 2]}, indent=3, sort_keys=True) + '\n'
   if arguments.manifest_only:
      if arguments.check:
         raise ValueError('Choose check or manifest-only')
      manifestPath.write_text(expectedManifest)
      print('Generated structural manifest only; no renderer or database accessed.')
      return
   expectedMarkdown = markdown(tables)
   if arguments.check:
      assert manifestPath.read_text() == expectedManifest, 'Stale structural registry JSON'
      assert (DIRECTORY / 'DatabaseSchema.md').read_text() == expectedMarkdown, 'Stale Markdown'
      svg = (DIRECTORY / 'DatabaseSchema.svg').read_text()
   else:
      svg = renderMermaid(tables)
   validateArtifacts(tables, svg)
   if not arguments.check:
      (DIRECTORY / 'DatabaseSchema.md').write_text(expectedMarkdown)
      (DIRECTORY / 'DatabaseSchema.svg').write_text(svg)
      manifestPath.write_text(expectedManifest)
   print(('Verified' if arguments.check else 'Generated') + ': ' + str(len(tables)) + ' tables, '
         + str(sum(len(table['columns']) for table in tables.values())) + ' columns, '
         + str(sum(len(table['foreignKeys']) for table in tables.values())) + ' SQL FKs, 2 logical references.')


if __name__ == '__main__':
   main()
