-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
REVOKE CREATE ON taskcompass_phase12_test.Pillar FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE CREATE ON taskcompass_phase12_test.Project FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE CREATE ON taskcompass_phase12_test.Objective FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE CREATE, INSERT, UPDATE ON taskcompass_phase12_test.MigrationAttempt FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE ALTER ON taskcompass_phase12_test.Task FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE ALTER ON taskcompass_phase12_test.CommandRecord FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE ALTER ON taskcompass_phase12_test.LocalChange FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE INSERT ON taskcompass_phase12_test.SchemaMigration FROM 'taskcompass_phase12_test'@'127.0.0.1';
REVOKE SELECT ON `taskcompass\_phase12\_test`.* FROM 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_phase12_test.Task TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_phase12_test.Pillar TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_phase12_test.Project TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_phase12_test.Objective TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT, INSERT ON taskcompass_phase12_test.CommandRecord TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT, INSERT ON taskcompass_phase12_test.LocalChange TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT ON taskcompass_phase12_test.Dataset TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT ON taskcompass_phase12_test.SchemaMigration TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT ON taskcompass_phase12_test.MigrationAttempt TO 'taskcompass_phase12_test'@'127.0.0.1';
