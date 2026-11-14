-- SPDX-License-Identifier: GPL-3.0-or-later
-- Review template only. Use setup.sh; never replace the token in a disk file.
-- The template is NOT directly executable SQL. The password is inserted in memory.
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_integration
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY {{TEST_PASSWORD_SQL_LITERAL}};
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_integration`.*
   TO 'taskcompass_test'@'127.0.0.1';
