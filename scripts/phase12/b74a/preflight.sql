-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7.4a populated-clone metadata-only guards; never mutate/access unrelated domain data.
SELECT 'SERVER',@@hostname,VERSION(),@@port,DATABASE(),CURRENT_USER(),@@lower_case_table_names,@@foreign_key_checks,@@check_constraint_checks;
SELECT 'STATE',
   (SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_phase12_test' AND DEFAULT_CHARACTER_SET_NAME='utf8mb4' AND DEFAULT_COLLATION_NAME='utf8mb4_unicode_ci'),
   (SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_phase12_test'),
   (SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_phase12_test'),
   (SELECT COUNT(*) FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_phase12_test'),
   (SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE DB='taskcompass_phase12_test' AND ID<>CONNECTION_ID());
SELECT 'OBJECT',LOWER(TABLE_NAME),TABLE_TYPE,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='taskcompass_phase12_test' ORDER BY TABLE_NAME;
SELECT 'HOST',Host FROM mysql.user WHERE User='taskcompass_phase12_test';
SELECT 'GLOBAL',COUNT(*) FROM information_schema.USER_PRIVILEGES
   WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_phase12_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39))
     AND (PRIVILEGE_TYPE<>'USAGE' OR IS_GRANTABLE<>'NO');
SELECT 'TABLE_RIGHT',TABLE_SCHEMA,LOWER(TABLE_NAME),PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES
   WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_phase12_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE;
SELECT 'COLUMN',COUNT(*) FROM information_schema.COLUMN_PRIVILEGES
   WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_phase12_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT 'ROLE',COUNT(*) FROM mysql.roles_mapping WHERE User='taskcompass_phase12_test';
SELECT 'RIGHT',TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES
   WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_phase12_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,PRIVILEGE_TYPE;
