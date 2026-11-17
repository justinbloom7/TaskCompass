-- SPDX-License-Identifier: GPL-3.0-or-later
-- DESTRUCTIVE: execute only after explicit approval for the reviewed Linux fixture.
-- Preconditions: feledria / MariaDB 10.5.29 / taskcompass_phase12_test;
-- exactly these five InnoDB tables, schema version 1, Dataset=1,
-- Task=0, CommandRecord=0, LocalChange=0; no other objects or writers.
-- Discards synthetic dataset/migration metadata. Preserves database/account/grants.
-- No IF EXISTS, FK bypass, transaction/rollback claim or automatic retry.
DROP TABLE taskcompass_phase12_test.LocalChange;
DROP TABLE taskcompass_phase12_test.CommandRecord;
DROP TABLE taskcompass_phase12_test.Task;
DROP TABLE taskcompass_phase12_test.Dataset;
DROP TABLE taskcompass_phase12_test.SchemaMigration;
