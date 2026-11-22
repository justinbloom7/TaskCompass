-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7.5 development metadata-only guards; never mutate/access unrelated domain data.
SELECT 'SERVER',@@hostname,VERSION(),@@port,DATABASE(),CURRENT_USER(),@@lower_case_table_names,@@foreign_key_checks,@@check_constraint_checks;
SELECT 'STATE',
   (SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_dev' AND DEFAULT_CHARACTER_SET_NAME='utf8mb4' AND DEFAULT_COLLATION_NAME='utf8mb4_unicode_ci'),
   (SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_dev'),
   (SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_dev'),
   (SELECT COUNT(*) FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_dev'),
   (SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE ID<>CONNECTION_ID() AND (DB='taskcompass_dev' OR USER IN ('taskcompass_dev','taskcompass_migrate','taskcompass_app')));
SELECT 'OBJECT',LOWER(TABLE_NAME),TABLE_TYPE,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='taskcompass_dev' ORDER BY TABLE_NAME;
SELECT 'HOST','taskcompass_dev',Host FROM mysql.user WHERE User='taskcompass_dev';
SELECT 'GLOBAL','taskcompass_dev',COUNT(*) FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) AND (PRIVILEGE_TYPE<>'USAGE' OR IS_GRANTABLE<>'NO');
SELECT 'COLUMN','taskcompass_dev',COUNT(*) FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT 'ROLE','taskcompass_dev',COUNT(*) FROM mysql.roles_mapping WHERE User='taskcompass_dev';
SELECT 'RIGHT','taskcompass_dev',TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,PRIVILEGE_TYPE;
SELECT 'TABLE_RIGHT','taskcompass_dev',TABLE_SCHEMA,LOWER(TABLE_NAME),PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE;
SELECT 'HOST','taskcompass_migrate',Host FROM mysql.user WHERE User='taskcompass_migrate';
SELECT 'GLOBAL','taskcompass_migrate',COUNT(*) FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_migrate',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) AND (PRIVILEGE_TYPE<>'USAGE' OR IS_GRANTABLE<>'NO');
SELECT 'COLUMN','taskcompass_migrate',COUNT(*) FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_migrate',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT 'ROLE','taskcompass_migrate',COUNT(*) FROM mysql.roles_mapping WHERE User='taskcompass_migrate';
SELECT 'RIGHT','taskcompass_migrate',TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_migrate',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,PRIVILEGE_TYPE;
SELECT 'TABLE_RIGHT','taskcompass_migrate',TABLE_SCHEMA,LOWER(TABLE_NAME),PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_migrate',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE;
SELECT 'HOST','taskcompass_app',Host FROM mysql.user WHERE User='taskcompass_app';
SELECT 'GLOBAL','taskcompass_app',COUNT(*) FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_app',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) AND (PRIVILEGE_TYPE<>'USAGE' OR IS_GRANTABLE<>'NO');
SELECT 'COLUMN','taskcompass_app',COUNT(*) FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_app',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT 'ROLE','taskcompass_app',COUNT(*) FROM mysql.roles_mapping WHERE User='taskcompass_app';
SELECT 'RIGHT','taskcompass_app',TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_app',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,PRIVILEGE_TYPE;
SELECT 'TABLE_RIGHT','taskcompass_app',TABLE_SCHEMA,LOWER(TABLE_NAME),PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=CONCAT(CHAR(39),'taskcompass_app',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39)) ORDER BY TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE;
