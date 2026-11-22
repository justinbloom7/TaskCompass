-- SPDX-License-Identifier: GPL-3.0-or-later
-- Read-only pre/post check for the specifically approved Phase 1.2 fixture recovery.
SELECT
   (SELECT COUNT(*) FROM information_schema.TABLES
      WHERE TABLE_SCHEMA='taskcompass_phase12_test') AS tablesRemaining,
   (SELECT COUNT(*) FROM information_schema.ROUTINES
      WHERE ROUTINE_SCHEMA='taskcompass_phase12_test') AS routinesRemaining,
   (SELECT COUNT(*) FROM information_schema.TRIGGERS
      WHERE TRIGGER_SCHEMA='taskcompass_phase12_test') AS triggersRemaining,
   (SELECT COUNT(*) FROM information_schema.EVENTS
      WHERE EVENT_SCHEMA='taskcompass_phase12_test') AS eventsRemaining;
