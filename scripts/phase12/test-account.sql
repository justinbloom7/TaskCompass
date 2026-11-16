-- SPDX-License-Identifier: GPL-3.0-or-later
-- Review template; password is substituted in memory by the fixed-target helper.
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_phase12_test
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_phase12_test'@'127.0.0.1'
   IDENTIFIED BY {{TEST_PASSWORD_SQL_LITERAL}};
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_phase12\_test`.*
   TO 'taskcompass_phase12_test'@'127.0.0.1';
