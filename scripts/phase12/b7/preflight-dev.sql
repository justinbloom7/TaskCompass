-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
SELECT @@hostname AS serverHost,VERSION() AS serverVersion,@@port AS serverPort,
   DATABASE() AS selectedDatabase,CURRENT_USER() AS authenticatedAccount,
   @@lower_case_table_names AS tableCaseMode,@@foreign_key_checks AS fkChecks,
   @@check_constraint_checks AS checkChecks;
SELECT SCHEMA_NAME,DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME
   FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_dev';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE FROM information_schema.TABLES
   WHERE TABLE_SCHEMA='taskcompass_dev' ORDER BY TABLE_NAME;
SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_dev';
SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_dev';
SELECT EVENT_NAME FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_dev';
SELECT ID,USER,HOST,DB,COMMAND,TIME FROM information_schema.PROCESSLIST
   WHERE DB='taskcompass_dev' AND ID<>CONNECTION_ID() ORDER BY ID;
SELECT version,checksum,schemaFingerprint,appliedAt FROM taskcompass_dev.SchemaMigration ORDER BY version;
SELECT id,createdAt FROM taskcompass_dev.Dataset ORDER BY id;
SELECT 'SchemaMigration' AS tableName,COUNT(*) AS rowCount FROM taskcompass_dev.SchemaMigration
UNION ALL SELECT 'Dataset' AS tableName,COUNT(*) AS rowCount FROM taskcompass_dev.Dataset
UNION ALL SELECT 'Task' AS tableName,COUNT(*) AS rowCount FROM taskcompass_dev.Task
UNION ALL SELECT 'CommandRecord' AS tableName,COUNT(*) AS rowCount FROM taskcompass_dev.CommandRecord
UNION ALL SELECT 'LocalChange' AS tableName,COUNT(*) AS rowCount FROM taskcompass_dev.LocalChange;
SELECT lifecycleStatus,(deletedAt IS NOT NULL) AS tombstoned,COUNT(*) AS rowCount FROM taskcompass_dev.Task GROUP BY lifecycleStatus,(deletedAt IS NOT NULL) ORDER BY lifecycleStatus,tombstoned;
SHOW CREATE TABLE taskcompass_dev.SchemaMigration;
SHOW CREATE TABLE taskcompass_dev.Dataset;
SHOW CREATE TABLE taskcompass_dev.Task;
SHOW CREATE TABLE taskcompass_dev.CommandRecord;
SHOW CREATE TABLE taskcompass_dev.LocalChange;
