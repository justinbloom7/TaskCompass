-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
GRANT CREATE ON taskcompass_dev.Pillar TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE ON taskcompass_dev.Project TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE ON taskcompass_dev.Objective TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT, UPDATE ON taskcompass_dev.MigrationAttempt TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.Task TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.CommandRecord TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.LocalChange TO 'taskcompass_migrate'@'127.0.0.1';
GRANT INSERT ON taskcompass_dev.SchemaMigration TO 'taskcompass_migrate'@'127.0.0.1';
