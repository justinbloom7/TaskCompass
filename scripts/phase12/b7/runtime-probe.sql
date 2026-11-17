-- SPDX-License-Identifier: GPL-3.0-or-later
-- B7 narrow-runtime permission rehearsal ONLY on taskcompass_phase12_test.
-- Requires its own approval. New client session, InnoDB tables, no --force.
-- This is a SQL privilege probe, not an application command/audit acceptance test.
-- All sample DML is rolled back; no DDL, grants, COMMIT, DELETE or history updates.
SET SESSION sql_mode='STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION';
SET SESSION foreign_key_checks=1;
SET SESSION check_constraint_checks=1;
START TRANSACTION;
SET @b7Dataset=(SELECT id FROM taskcompass_phase12_test.Dataset);
SET @b7Time=CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS SIGNED);
SET @b7Pillar='d900c962-6257-468a-b3c2-171ba1ae6c35';
SET @b7Project='34a32335-c2eb-44b2-81c2-ae283d349961';
SET @b7Objective='0dda8b35-b148-45fd-afe0-30d36cdf478d';
SET @b7Task='686ae2ef-a418-461f-9c00-0256d00566b6';
SET @b7Command='3b298b31-4f6c-412a-a148-80fc80a11550';
SET @b7Operation='83062ee1-97ad-4cfd-bdb1-c1ab7a78204d';
INSERT INTO taskcompass_phase12_test.Pillar
   (id,datasetId,name,createdAt,updatedAt,localRevision)
   VALUES (@b7Pillar,@b7Dataset,'B7 temporary privilege probe',@b7Time,@b7Time,1);
INSERT INTO taskcompass_phase12_test.Project
   (id,datasetId,pillarId,title,createdAt,updatedAt,localRevision)
   VALUES (@b7Project,@b7Dataset,@b7Pillar,'B7 temporary project',@b7Time,@b7Time,1);
INSERT INTO taskcompass_phase12_test.Objective
   (id,datasetId,projectId,title,createdAt,updatedAt,localRevision)
   VALUES (@b7Objective,@b7Dataset,@b7Project,'B7 temporary objective',@b7Time,@b7Time,1);
INSERT INTO taskcompass_phase12_test.Task
   (id,datasetId,title,lifecycleStatus,createdAt,updatedAt,localRevision,objectiveId)
   VALUES (@b7Task,@b7Dataset,'B7 temporary task','active',@b7Time,@b7Time,1,@b7Objective);
SET @b7Snapshot=JSON_OBJECT('entityType','task','id',@b7Task,'datasetId',@b7Dataset,
   'title','B7 temporary task','notes',NULL,'lifecycleStatus','active',
   'createdAt',CAST(@b7Time AS CHAR),'updatedAt',CAST(@b7Time AS CHAR),
   'completedAt',NULL,'deletedAt',NULL,'localRevision','1',
   'objectiveId',@b7Objective,'directProjectId',NULL,'changedFields',16,
   'dependencies',JSON_ARRAY());
INSERT INTO taskcompass_phase12_test.CommandRecord
   (id,operationId,datasetId,taskId,commandType,payloadVersion,recordedAt,
    beforeSnapshot,afterSnapshot,resultingRevision)
   VALUES (@b7Command,@b7Operation,@b7Dataset,@b7Task,'create',2,@b7Time,NULL,@b7Snapshot,1);
INSERT INTO taskcompass_phase12_test.LocalChange
   (id,operationId,datasetId,taskId,localRevision,payloadVersion,immutablePayload,createdAt,acknowledgmentState)
   VALUES (@b7Command,@b7Operation,@b7Dataset,@b7Task,1,2,@b7Snapshot,@b7Time,'pending');
UPDATE taskcompass_phase12_test.Pillar SET name='B7 updated probe',updatedAt=@b7Time+1,localRevision=2 WHERE id=@b7Pillar;
SELECT ROW_COUNT() AS pillarUpdates;
UPDATE taskcompass_phase12_test.Project SET title='B7 updated probe',updatedAt=@b7Time+1,localRevision=2 WHERE id=@b7Project;
SELECT ROW_COUNT() AS projectUpdates;
UPDATE taskcompass_phase12_test.Objective SET title='B7 updated probe',updatedAt=@b7Time+1,localRevision=2 WHERE id=@b7Objective;
SELECT ROW_COUNT() AS objectiveUpdates;
UPDATE taskcompass_phase12_test.Task SET title='B7 updated probe',updatedAt=@b7Time+1,localRevision=2 WHERE id=@b7Task;
SELECT ROW_COUNT() AS taskUpdates;
ROLLBACK;
SELECT 'Pillar' AS probeTable,COUNT(*) AS remaining FROM taskcompass_phase12_test.Pillar WHERE id=@b7Pillar
UNION ALL SELECT 'Project',COUNT(*) FROM taskcompass_phase12_test.Project WHERE id=@b7Project
UNION ALL SELECT 'Objective',COUNT(*) FROM taskcompass_phase12_test.Objective WHERE id=@b7Objective
UNION ALL SELECT 'Task',COUNT(*) FROM taskcompass_phase12_test.Task WHERE id=@b7Task
UNION ALL SELECT 'CommandRecord',COUNT(*) FROM taskcompass_phase12_test.CommandRecord WHERE id=@b7Command
UNION ALL SELECT 'LocalChange',COUNT(*) FROM taskcompass_phase12_test.LocalChange WHERE id=@b7Command;
