-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
-- Only after approved snapshot import and verified exact six existing schema grants.
REVOKE SELECT, INSERT, UPDATE, CREATE, DROP, ALTER ON `taskcompass\_phase12\_test`.* FROM 'taskcompass_phase12_test'@'127.0.0.1';
GRANT SELECT ON `taskcompass\_phase12\_test`.* TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE ON taskcompass_phase12_test.Pillar TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE ON taskcompass_phase12_test.Project TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE ON taskcompass_phase12_test.Objective TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT CREATE, INSERT, UPDATE ON taskcompass_phase12_test.MigrationAttempt TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT ALTER ON taskcompass_phase12_test.Task TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT ALTER ON taskcompass_phase12_test.CommandRecord TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT ALTER ON taskcompass_phase12_test.LocalChange TO 'taskcompass_phase12_test'@'127.0.0.1';
GRANT INSERT ON taskcompass_phase12_test.SchemaMigration TO 'taskcompass_phase12_test'@'127.0.0.1';
