-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
-- Selected database is approved and must match the export being compared.
-- Session-only rendering controls; no schema/data mutation.
SET SESSION sql_quote_show_create=1;
SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION';
SHOW CREATE TABLE SchemaMigration;
SHOW CREATE TABLE Dataset;
SHOW CREATE TABLE Task;
SHOW CREATE TABLE CommandRecord;
SHOW CREATE TABLE LocalChange;
SHOW CREATE TABLE MigrationAttempt;
SHOW CREATE TABLE Pillar;
SHOW CREATE TABLE Project;
SHOW CREATE TABLE Objective;
