-- SPDX-License-Identifier: GPL-3.0-or-later
-- Read-only retained-fixture inspection, fixed Phase 1.2 target only.
-- No account information, task text, mutations, cleanup or recovery SQL.
SELECT @@hostname AS serverHost, VERSION() AS serverVersion,
   @@port AS serverPort, DATABASE() AS selectedDatabase;
SELECT TABLE_NAME,TABLE_TYPE,ENGINE
   FROM information_schema.TABLES
   WHERE TABLE_SCHEMA='taskcompass_phase12_test'
   ORDER BY TABLE_NAME;
SELECT version,checksum,schemaFingerprint,appliedAt
   FROM taskcompass_phase12_test.SchemaMigration ORDER BY version;
SELECT 'Dataset' AS tableName,COUNT(*) AS rowCount FROM taskcompass_phase12_test.Dataset
   UNION ALL SELECT 'Task',COUNT(*) FROM taskcompass_phase12_test.Task
   UNION ALL SELECT 'CommandRecord',COUNT(*) FROM taskcompass_phase12_test.CommandRecord
   UNION ALL SELECT 'LocalChange',COUNT(*) FROM taskcompass_phase12_test.LocalChange;
