-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
-- Fresh CLI-only restore target, never add it to application identity allowlists.
-- Render password token in memory; never write the actual password to this file.
SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE taskcompass_phase12_restore CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_phase12_restore'@'127.0.0.1' IDENTIFIED BY {{PASSWORD_SQL_LITERAL}};
GRANT SELECT, INSERT, CREATE ON `taskcompass\_phase12\_restore`.* TO 'taskcompass_phase12_restore'@'127.0.0.1';
