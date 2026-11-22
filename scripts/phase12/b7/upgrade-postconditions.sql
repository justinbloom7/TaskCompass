-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
-- Run immediately after migration, before any runtime/GUI mutation.
SELECT 'Pillar' AS tableName,COUNT(*) AS rowCount FROM Pillar
UNION ALL SELECT 'Project',COUNT(*) FROM Project
UNION ALL SELECT 'Objective',COUNT(*) FROM Objective;
SELECT COUNT(*) AS oldTasksWithUnexpectedOwner FROM Task WHERE objectiveId IS NOT NULL OR directProjectId IS NOT NULL;
SELECT COUNT(*) AS oldCommandsWithUnexpectedTarget FROM CommandRecord WHERE taskId IS NULL OR pillarId IS NOT NULL OR projectId IS NOT NULL OR objectiveId IS NOT NULL OR payloadVersion<>1;
SELECT COUNT(*) AS oldChangesWithUnexpectedTarget FROM LocalChange WHERE taskId IS NULL OR pillarId IS NOT NULL OR projectId IS NOT NULL OR objectiveId IS NOT NULL OR payloadVersion<>1;
SELECT version,checksum,schemaFingerprint,appliedAt FROM SchemaMigration ORDER BY version;
SELECT version,checksum,baselineFingerprint,startedAt,completedAt,lastCompletedStep,attemptState FROM MigrationAttempt ORDER BY version;
