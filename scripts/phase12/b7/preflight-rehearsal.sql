-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
SELECT @@hostname AS serverHost,VERSION() AS serverVersion,@@port AS serverPort,
   DATABASE() AS selectedDatabase,CURRENT_USER() AS authenticatedAccount,
   @@lower_case_table_names AS tableCaseMode,@@foreign_key_checks AS fkChecks,
   @@check_constraint_checks AS checkChecks;
SELECT SCHEMA_NAME,DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME
   FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_phase12_test';
SELECT TABLE_NAME,TABLE_TYPE,ENGINE FROM information_schema.TABLES
   WHERE TABLE_SCHEMA='taskcompass_phase12_test' ORDER BY TABLE_NAME;
SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_phase12_test';
SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_phase12_test';
SELECT EVENT_NAME FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_phase12_test';
SELECT ID,USER,HOST,DB,COMMAND,TIME FROM information_schema.PROCESSLIST
   WHERE DB='taskcompass_phase12_test' AND ID<>CONNECTION_ID() ORDER BY ID;
