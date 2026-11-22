-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 review package only: execution requires the matching explicit approval.
-- Selected database must be explicitly approved dev/rehearsal/restore; read-only.
SELECT 'COUNT:SchemaMigration',COUNT(*) FROM SchemaMigration;
SELECT 'SchemaMigration' AS tableName,
   IF(version IS NULL,'N',CONCAT('I',CAST(version AS CHAR CHARACTER SET ascii))) AS version,
   IF(checksum IS NULL,'N',CONCAT('H',HEX(checksum))) AS checksum,
   IF(schemaFingerprint IS NULL,'N',CONCAT('H',HEX(schemaFingerprint))) AS schemaFingerprint,
   IF(appliedAt IS NULL,'N',CONCAT('I',CAST(appliedAt AS CHAR CHARACTER SET ascii))) AS appliedAt
   FROM SchemaMigration ORDER BY version;
SELECT 'COUNT:Dataset',COUNT(*) FROM Dataset;
SELECT 'Dataset' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(name IS NULL,'N',CONCAT('H',HEX(name))) AS name,
   IF(createdAt IS NULL,'N',CONCAT('I',CAST(createdAt AS CHAR CHARACTER SET ascii))) AS createdAt
   FROM Dataset ORDER BY id;
SELECT 'COUNT:Task',COUNT(*) FROM Task;
SELECT 'Task' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(datasetId IS NULL,'N',CONCAT('H',HEX(datasetId))) AS datasetId,
   IF(title IS NULL,'N',CONCAT('H',HEX(title))) AS title,
   IF(notes IS NULL,'N',CONCAT('H',HEX(notes))) AS notes,
   IF(lifecycleStatus IS NULL,'N',CONCAT('H',HEX(lifecycleStatus))) AS lifecycleStatus,
   IF(createdAt IS NULL,'N',CONCAT('I',CAST(createdAt AS CHAR CHARACTER SET ascii))) AS createdAt,
   IF(updatedAt IS NULL,'N',CONCAT('I',CAST(updatedAt AS CHAR CHARACTER SET ascii))) AS updatedAt,
   IF(completedAt IS NULL,'N',CONCAT('I',CAST(completedAt AS CHAR CHARACTER SET ascii))) AS completedAt,
   IF(deletedAt IS NULL,'N',CONCAT('I',CAST(deletedAt AS CHAR CHARACTER SET ascii))) AS deletedAt,
   IF(localRevision IS NULL,'N',CONCAT('I',CAST(localRevision AS CHAR CHARACTER SET ascii))) AS localRevision,
   IF(objectiveId IS NULL,'N',CONCAT('H',HEX(objectiveId))) AS objectiveId,
   IF(directProjectId IS NULL,'N',CONCAT('H',HEX(directProjectId))) AS directProjectId
   FROM Task ORDER BY id;
SELECT 'COUNT:CommandRecord',COUNT(*) FROM CommandRecord;
SELECT 'CommandRecord' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(operationId IS NULL,'N',CONCAT('H',HEX(operationId))) AS operationId,
   IF(datasetId IS NULL,'N',CONCAT('H',HEX(datasetId))) AS datasetId,
   IF(taskId IS NULL,'N',CONCAT('H',HEX(taskId))) AS taskId,
   IF(commandType IS NULL,'N',CONCAT('H',HEX(commandType))) AS commandType,
   IF(payloadVersion IS NULL,'N',CONCAT('I',CAST(payloadVersion AS CHAR CHARACTER SET ascii))) AS payloadVersion,
   IF(recordedAt IS NULL,'N',CONCAT('I',CAST(recordedAt AS CHAR CHARACTER SET ascii))) AS recordedAt,
   IF(beforeSnapshot IS NULL,'N',CONCAT('H',HEX(beforeSnapshot))) AS beforeSnapshot,
   IF(afterSnapshot IS NULL,'N',CONCAT('H',HEX(afterSnapshot))) AS afterSnapshot,
   IF(resultingRevision IS NULL,'N',CONCAT('I',CAST(resultingRevision AS CHAR CHARACTER SET ascii))) AS resultingRevision,
   IF(compensatesCommandId IS NULL,'N',CONCAT('H',HEX(compensatesCommandId))) AS compensatesCommandId,
   IF(reappliesCommandId IS NULL,'N',CONCAT('H',HEX(reappliesCommandId))) AS reappliesCommandId,
   IF(pillarId IS NULL,'N',CONCAT('H',HEX(pillarId))) AS pillarId,
   IF(projectId IS NULL,'N',CONCAT('H',HEX(projectId))) AS projectId,
   IF(objectiveId IS NULL,'N',CONCAT('H',HEX(objectiveId))) AS objectiveId
   FROM CommandRecord ORDER BY id;
SELECT 'COUNT:LocalChange',COUNT(*) FROM LocalChange;
SELECT 'LocalChange' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(operationId IS NULL,'N',CONCAT('H',HEX(operationId))) AS operationId,
   IF(datasetId IS NULL,'N',CONCAT('H',HEX(datasetId))) AS datasetId,
   IF(taskId IS NULL,'N',CONCAT('H',HEX(taskId))) AS taskId,
   IF(localRevision IS NULL,'N',CONCAT('I',CAST(localRevision AS CHAR CHARACTER SET ascii))) AS localRevision,
   IF(payloadVersion IS NULL,'N',CONCAT('I',CAST(payloadVersion AS CHAR CHARACTER SET ascii))) AS payloadVersion,
   IF(immutablePayload IS NULL,'N',CONCAT('H',HEX(immutablePayload))) AS immutablePayload,
   IF(createdAt IS NULL,'N',CONCAT('I',CAST(createdAt AS CHAR CHARACTER SET ascii))) AS createdAt,
   IF(acknowledgmentState IS NULL,'N',CONCAT('H',HEX(acknowledgmentState))) AS acknowledgmentState,
   IF(pillarId IS NULL,'N',CONCAT('H',HEX(pillarId))) AS pillarId,
   IF(projectId IS NULL,'N',CONCAT('H',HEX(projectId))) AS projectId,
   IF(objectiveId IS NULL,'N',CONCAT('H',HEX(objectiveId))) AS objectiveId
   FROM LocalChange ORDER BY id;
SELECT 'COUNT:MigrationAttempt',COUNT(*) FROM MigrationAttempt;
SELECT 'MigrationAttempt' AS tableName,
   IF(version IS NULL,'N',CONCAT('I',CAST(version AS CHAR CHARACTER SET ascii))) AS version,
   IF(checksum IS NULL,'N',CONCAT('H',HEX(checksum))) AS checksum,
   IF(baselineFingerprint IS NULL,'N',CONCAT('H',HEX(baselineFingerprint))) AS baselineFingerprint,
   IF(startedAt IS NULL,'N',CONCAT('I',CAST(startedAt AS CHAR CHARACTER SET ascii))) AS startedAt,
   IF(completedAt IS NULL,'N',CONCAT('I',CAST(completedAt AS CHAR CHARACTER SET ascii))) AS completedAt,
   IF(lastCompletedStep IS NULL,'N',CONCAT('I',CAST(lastCompletedStep AS CHAR CHARACTER SET ascii))) AS lastCompletedStep,
   IF(attemptState IS NULL,'N',CONCAT('H',HEX(attemptState))) AS attemptState
   FROM MigrationAttempt ORDER BY version;
SELECT 'COUNT:Pillar',COUNT(*) FROM Pillar;
SELECT 'Pillar' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(datasetId IS NULL,'N',CONCAT('H',HEX(datasetId))) AS datasetId,
   IF(parentPillarId IS NULL,'N',CONCAT('H',HEX(parentPillarId))) AS parentPillarId,
   IF(name IS NULL,'N',CONCAT('H',HEX(name))) AS name,
   IF(description IS NULL,'N',CONCAT('H',HEX(description))) AS description,
   IF(archivedAt IS NULL,'N',CONCAT('I',CAST(archivedAt AS CHAR CHARACTER SET ascii))) AS archivedAt,
   IF(createdAt IS NULL,'N',CONCAT('I',CAST(createdAt AS CHAR CHARACTER SET ascii))) AS createdAt,
   IF(updatedAt IS NULL,'N',CONCAT('I',CAST(updatedAt AS CHAR CHARACTER SET ascii))) AS updatedAt,
   IF(deletedAt IS NULL,'N',CONCAT('I',CAST(deletedAt AS CHAR CHARACTER SET ascii))) AS deletedAt,
   IF(localRevision IS NULL,'N',CONCAT('I',CAST(localRevision AS CHAR CHARACTER SET ascii))) AS localRevision
   FROM Pillar ORDER BY id;
SELECT 'COUNT:Project',COUNT(*) FROM Project;
SELECT 'Project' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(datasetId IS NULL,'N',CONCAT('H',HEX(datasetId))) AS datasetId,
   IF(pillarId IS NULL,'N',CONCAT('H',HEX(pillarId))) AS pillarId,
   IF(parentProjectId IS NULL,'N',CONCAT('H',HEX(parentProjectId))) AS parentProjectId,
   IF(title IS NULL,'N',CONCAT('H',HEX(title))) AS title,
   IF(description IS NULL,'N',CONCAT('H',HEX(description))) AS description,
   IF(archivedAt IS NULL,'N',CONCAT('I',CAST(archivedAt AS CHAR CHARACTER SET ascii))) AS archivedAt,
   IF(createdAt IS NULL,'N',CONCAT('I',CAST(createdAt AS CHAR CHARACTER SET ascii))) AS createdAt,
   IF(updatedAt IS NULL,'N',CONCAT('I',CAST(updatedAt AS CHAR CHARACTER SET ascii))) AS updatedAt,
   IF(deletedAt IS NULL,'N',CONCAT('I',CAST(deletedAt AS CHAR CHARACTER SET ascii))) AS deletedAt,
   IF(localRevision IS NULL,'N',CONCAT('I',CAST(localRevision AS CHAR CHARACTER SET ascii))) AS localRevision
   FROM Project ORDER BY id;
SELECT 'COUNT:Objective',COUNT(*) FROM Objective;
SELECT 'Objective' AS tableName,
   IF(id IS NULL,'N',CONCAT('H',HEX(id))) AS id,
   IF(datasetId IS NULL,'N',CONCAT('H',HEX(datasetId))) AS datasetId,
   IF(pillarId IS NULL,'N',CONCAT('H',HEX(pillarId))) AS pillarId,
   IF(projectId IS NULL,'N',CONCAT('H',HEX(projectId))) AS projectId,
   IF(title IS NULL,'N',CONCAT('H',HEX(title))) AS title,
   IF(description IS NULL,'N',CONCAT('H',HEX(description))) AS description,
   IF(archivedAt IS NULL,'N',CONCAT('I',CAST(archivedAt AS CHAR CHARACTER SET ascii))) AS archivedAt,
   IF(createdAt IS NULL,'N',CONCAT('I',CAST(createdAt AS CHAR CHARACTER SET ascii))) AS createdAt,
   IF(updatedAt IS NULL,'N',CONCAT('I',CAST(updatedAt AS CHAR CHARACTER SET ascii))) AS updatedAt,
   IF(deletedAt IS NULL,'N',CONCAT('I',CAST(deletedAt AS CHAR CHARACTER SET ascii))) AS deletedAt,
   IF(localRevision IS NULL,'N',CONCAT('I',CAST(localRevision AS CHAR CHARACTER SET ascii))) AS localRevision
   FROM Objective ORDER BY id;
