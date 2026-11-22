-- SPDX-License-Identifier: GPL-3.0-or-later
-- Metadata only. Administrator session deliberately has no selected database.
SELECT 'SERVER',@@hostname,VERSION(),@@port,IFNULL(DATABASE(),'NULL'),CURRENT_USER(),@@lower_case_table_names,@@foreign_key_checks,@@check_constraint_checks;
SELECT 'DATABASE',SCHEMA_NAME,DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME FROM information_schema.SCHEMATA WHERE LOWER(SCHEMA_NAME)='taskcompass_phase12_restore';
SELECT 'ACCOUNT',User,Host,is_role FROM mysql.user WHERE LOWER(User)='taskcompass_phase12_restore';
SELECT 'GLOBAL',PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.USER_PRIVILEGES WHERE GRANTEE="'taskcompass_phase12_restore'@'127.0.0.1'";
SELECT 'RIGHT',TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE="'taskcompass_phase12_restore'@'127.0.0.1'";
SELECT 'EXTRA',
 (SELECT COUNT(*) FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE="'taskcompass_phase12_restore'@'127.0.0.1'"),
 (SELECT COUNT(*) FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE="'taskcompass_phase12_restore'@'127.0.0.1'"),
 (SELECT COUNT(*) FROM mysql.roles_mapping WHERE LOWER(User)='taskcompass_phase12_restore'),
 (SELECT COUNT(*) FROM mysql.proxies_priv WHERE LOWER(User)='taskcompass_phase12_restore' OR LOWER(Proxied_user)='taskcompass_phase12_restore'),
 (SELECT COUNT(*) FROM mysql.procs_priv WHERE LOWER(User)='taskcompass_phase12_restore');
SELECT 'OBJECT',LOWER(TABLE_NAME),TABLE_TYPE,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='taskcompass_phase12_restore';
SELECT 'COLUMN',LOWER(TABLE_NAME),COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='taskcompass_phase12_restore';
SELECT 'INVALID',
 (SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_phase12_restore'),
 (SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_phase12_restore'),
 (SELECT COUNT(*) FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_phase12_restore'),
 (SELECT COUNT(*) FROM information_schema.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA='taskcompass_phase12_restore' AND REFERENCED_TABLE_NAME IS NOT NULL AND REFERENCED_TABLE_SCHEMA<>'taskcompass_phase12_restore'),
 (SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE ID<>CONNECTION_ID() AND ID<>@b79_reader_id AND (DB='taskcompass_phase12_restore' OR USER='taskcompass_phase12_restore'));
