CREATE TABLE Context (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   name VARCHAR(1024) NOT NULL,
   sortKey VARBINARY(128) NOT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   archivedAt BIGINT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   UNIQUE KEY contextIdentity (datasetId, id),
   INDEX contextOrder (datasetId, deletedAt, sortKey, id),
   CONSTRAINT contextDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT contextIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT contextName CHECK (OCTET_LENGTH(name) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(name)) > 0),
   CONSTRAINT contextRank CHECK (HEX(sortKey) REGEXP '^(30|31)*31$'),
   CONSTRAINT contextTimes CHECK (createdAt > 0 AND updatedAt >= createdAt
      AND (archivedAt IS NULL OR archivedAt BETWEEN createdAt AND updatedAt)
      AND (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT contextRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE TaskContext (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   contextId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   liveTaskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin
      GENERATED ALWAYS AS (CASE WHEN deletedAt IS NULL THEN RTRIM(taskId) ELSE NULL END) PERSISTENT,
   UNIQUE KEY taskContextOneLive (datasetId, liveTaskId),
   INDEX taskContextByTask (datasetId, taskId, deletedAt),
   INDEX taskContextByContext (datasetId, contextId, deletedAt, taskId),
   CONSTRAINT taskContextTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT taskContextContext FOREIGN KEY (datasetId, contextId) REFERENCES Context(datasetId, id),
   CONSTRAINT taskContextIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT taskContextTimes CHECK (createdAt > 0 AND updatedAt >= createdAt
      AND (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT taskContextRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

ALTER TABLE Task
   ADD COLUMN priority BIGINT NOT NULL DEFAULT 5,
   ADD COLUMN manualOrderKey VARBINARY(128) NULL,
   ADD CONSTRAINT taskPriority CHECK (priority > 0);

ALTER TABLE Task
   MODIFY COLUMN manualOrderKey VARBINARY(128) NOT NULL,
   ADD CONSTRAINT taskManualRank CHECK (HEX(manualOrderKey) REGEXP '^(30|31)*31$'),
   ADD INDEX taskBandOrder (datasetId, deletedAt, priority, manualOrderKey, id);

ALTER TABLE CommandRecord
   ADD COLUMN contextId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   DROP CONSTRAINT commandTargetV2,
   DROP CONSTRAINT commandPayloadV2,
   DROP CONSTRAINT commandKindV2,
   ADD UNIQUE KEY commandDatasetIdentity (datasetId, id),
   ADD INDEX commandContextHistory (datasetId, contextId, resultingRevision),
   ADD CONSTRAINT commandContextV3 FOREIGN KEY (datasetId, contextId) REFERENCES Context(datasetId, id),
   ADD CONSTRAINT commandTargetV3 CHECK (
      (taskId IS NOT NULL) + (pillarId IS NOT NULL) + (projectId IS NOT NULL)
      + (objectiveId IS NOT NULL) + (contextId IS NOT NULL) = 1
      AND (payloadVersion = 3 OR contextId IS NULL)),
   ADD CONSTRAINT commandPayloadV3 CHECK (
      ((payloadVersion = 1 AND taskId IS NOT NULL) OR payloadVersion IN (2,3))
      AND resultingRevision > 0 AND JSON_VALID(afterSnapshot)
      AND (beforeSnapshot IS NULL OR JSON_VALID(beforeSnapshot))),
   ADD CONSTRAINT commandKindV3 CHECK (
      commandType IN ('create','edit','move','archive','restore','complete','reopen','undo','redo')
      OR (payloadVersion = 3 AND (
         (commandType = 'delete' AND contextId IS NOT NULL)
         OR (commandType = 'reorder' AND (taskId IS NOT NULL OR contextId IS NOT NULL))
         OR (taskId IS NOT NULL AND commandType IN
            ('assign','work','correct','day-close','day-resume')))));

ALTER TABLE LocalChange
   ADD COLUMN contextId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT NULL,
   DROP CONSTRAINT changeTargetV2,
   DROP CONSTRAINT changePayloadV2,
   ADD INDEX changeContext (datasetId, contextId),
   ADD CONSTRAINT changeContextV3 FOREIGN KEY (datasetId, contextId) REFERENCES Context(datasetId, id),
   ADD CONSTRAINT changeTargetV3 CHECK (
      (taskId IS NOT NULL) + (pillarId IS NOT NULL) + (projectId IS NOT NULL)
      + (objectiveId IS NOT NULL) + (contextId IS NOT NULL) = 1
      AND (payloadVersion = 3 OR contextId IS NULL)),
   ADD CONSTRAINT changePayloadV3 CHECK (
      ((payloadVersion = 1 AND taskId IS NOT NULL) OR payloadVersion IN (2,3))
      AND localRevision > 0 AND JSON_VALID(immutablePayload));

CREATE TABLE WorkEntry (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   commandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   workDate DATE NOT NULL,
   timeZone VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   recordedAt BIGINT NOT NULL,
   kind VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   note MEDIUMTEXT NULL,
   correctsEntryId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   voidedAt BIGINT NULL,
   voidedByCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   localRevision BIGINT NOT NULL,
   liveQuickTaskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin
      GENERATED ALWAYS AS (CASE WHEN kind = 'quick' AND voidedAt IS NULL THEN RTRIM(taskId) ELSE NULL END) PERSISTENT,
   UNIQUE KEY workTaskIdentity (datasetId, taskId, id),
   UNIQUE KEY workOneQuick (datasetId, workDate, liveQuickTaskId),
   INDEX workHistory (datasetId, taskId, workDate, recordedAt, id),
   INDEX workByCommand (datasetId, commandId),
   INDEX workByCorrection (datasetId, taskId, correctsEntryId),
   INDEX workByVoidCommand (datasetId, voidedByCommandId),
   CONSTRAINT workTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT workCommand FOREIGN KEY (datasetId, commandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT workCorrection FOREIGN KEY (datasetId, taskId, correctsEntryId) REFERENCES WorkEntry(datasetId, taskId, id),
   CONSTRAINT workVoidCommand FOREIGN KEY (datasetId, voidedByCommandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT workIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT workDateZone CHECK (YEAR(workDate) BETWEEN 1000 AND 9999
      AND MONTH(workDate) BETWEEN 1 AND 12 AND DAYOFMONTH(workDate) BETWEEN 1 AND 31
      AND OCTET_LENGTH(timeZone) > 0),
   CONSTRAINT workKind CHECK (kind IN ('quick','session','closure','completion','correction')
      AND ((kind = 'correction') = (correctsEntryId IS NOT NULL))
      AND (correctsEntryId IS NULL OR correctsEntryId <> id)),
   CONSTRAINT workNote CHECK (note IS NULL OR OCTET_LENGTH(note) <= 65535),
   CONSTRAINT workTimes CHECK (recordedAt > 0
      AND ((voidedAt IS NULL) = (voidedByCommandId IS NULL))
      AND (voidedAt IS NULL OR (kind <> 'correction' AND voidedAt >= recordedAt))),
   CONSTRAINT workRevision CHECK (localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE TaskDayDisposition (
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   workDate DATE NOT NULL,
   timeZone VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   dayStatus VARCHAR(8) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   closureCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   lastCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   updatedAt BIGINT NOT NULL,
   localRevision BIGINT NOT NULL,
   PRIMARY KEY (datasetId, taskId, workDate),
   INDEX dayByStatus (datasetId, workDate, dayStatus, taskId),
   INDEX dayByClosure (datasetId, closureCommandId),
   INDEX dayByCommand (datasetId, lastCommandId),
   CONSTRAINT dayTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT dayClosure FOREIGN KEY (datasetId, closureCommandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT dayCommand FOREIGN KEY (datasetId, lastCommandId) REFERENCES CommandRecord(datasetId, id),
   CONSTRAINT dayDateZone CHECK (YEAR(workDate) BETWEEN 1000 AND 9999
      AND MONTH(workDate) BETWEEN 1 AND 12 AND DAYOFMONTH(workDate) BETWEEN 1 AND 31
      AND OCTET_LENGTH(timeZone) > 0),
   CONSTRAINT dayState CHECK (
      (dayStatus = 'open' AND closureCommandId IS NULL)
      OR (dayStatus = 'closed' AND closureCommandId IS NOT NULL)),
   CONSTRAINT dayRevision CHECK (updatedAt > 0 AND localRevision > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
