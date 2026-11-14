-- SPDX-License-Identifier: GPL-3.0-or-later
-- Review template only. No existing account is overwritten.
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE USER 'taskcompass_app'@'127.0.0.1' IDENTIFIED BY {{PASSWORD}};
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Task TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT ON taskcompass_dev.CommandRecord TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT ON taskcompass_dev.LocalChange TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT ON taskcompass_dev.Dataset TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT ON taskcompass_dev.SchemaMigration TO 'taskcompass_app'@'127.0.0.1';
REVOKE CREATE ON `taskcompass\_dev`.* FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT ON taskcompass_dev.Dataset FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT ON taskcompass_dev.SchemaMigration FROM 'taskcompass_migrate'@'127.0.0.1';
