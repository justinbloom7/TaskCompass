-- SPDX-License-Identifier: GPL-3.0-or-later
-- Review template only. The runner supplies the password in memory, never on disk.
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE USER 'taskcompass_migrate'@'127.0.0.1' IDENTIFIED BY {{PASSWORD}};
GRANT CREATE, SELECT ON `taskcompass\_dev`.* TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT ON taskcompass_dev.Dataset TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT ON taskcompass_dev.SchemaMigration TO 'taskcompass_migrate'@'127.0.0.1';
