CREATE TABLE SchemaMigration (
   version INT NOT NULL PRIMARY KEY,
   checksum CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   schemaFingerprint CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   appliedAt BIGINT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Dataset (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   name VARCHAR(255) NOT NULL,
   createdAt BIGINT NOT NULL,
   CONSTRAINT datasetIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT datasetCreatedAt CHECK (createdAt > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE Task (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   title VARCHAR(1024) NOT NULL,
   notes MEDIUMTEXT NULL,
   lifecycleStatus VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   createdAt BIGINT NOT NULL,
   updatedAt BIGINT NOT NULL,
   completedAt BIGINT NULL,
   deletedAt BIGINT NULL,
   localRevision BIGINT NOT NULL,
   CONSTRAINT taskDataset FOREIGN KEY (datasetId) REFERENCES Dataset(id),
   CONSTRAINT taskIdFormat CHECK (id REGEXP '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'),
   CONSTRAINT taskTitle CHECK (OCTET_LENGTH(title) BETWEEN 1 AND 1024 AND CHAR_LENGTH(TRIM(title)) > 0),
   CONSTRAINT taskNotes CHECK (notes IS NULL OR OCTET_LENGTH(notes) <= 65535),
   CONSTRAINT taskLifecycle CHECK ((lifecycleStatus = 'active' AND completedAt IS NULL) OR (lifecycleStatus = 'completed' AND completedAt IS NOT NULL)),
   CONSTRAINT taskTimes CHECK (createdAt > 0 AND updatedAt >= createdAt AND (completedAt IS NULL OR completedAt BETWEEN createdAt AND updatedAt) AND (deletedAt IS NULL OR deletedAt BETWEEN createdAt AND updatedAt)),
   CONSTRAINT taskRevision CHECK (localRevision > 0),
   INDEX taskActive (datasetId, deletedAt, lifecycleStatus),
   UNIQUE KEY taskDatasetIdentity (datasetId, id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE CommandRecord (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   operationId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   commandType VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   payloadVersion INT NOT NULL,
   recordedAt BIGINT NOT NULL,
   beforeSnapshot MEDIUMTEXT NULL,
   afterSnapshot MEDIUMTEXT NOT NULL,
   resultingRevision BIGINT NOT NULL,
   compensatesCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   reappliesCommandId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NULL,
   CONSTRAINT commandTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT commandPayload CHECK (payloadVersion = 1 AND resultingRevision > 0 AND JSON_VALID(afterSnapshot) AND (beforeSnapshot IS NULL OR JSON_VALID(beforeSnapshot))),
   CONSTRAINT commandKind CHECK (commandType IN ('create', 'edit', 'complete', 'reopen', 'undo', 'redo')),
   INDEX commandTaskHistory (datasetId, taskId, resultingRevision)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE LocalChange (
   id CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
   operationId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
   datasetId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   taskId CHAR(36) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
   localRevision BIGINT NOT NULL,
   payloadVersion INT NOT NULL,
   immutablePayload MEDIUMTEXT NOT NULL,
   createdAt BIGINT NOT NULL,
   acknowledgmentState VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'pending',
   CONSTRAINT changeTask FOREIGN KEY (datasetId, taskId) REFERENCES Task(datasetId, id),
   CONSTRAINT changeCommand FOREIGN KEY (operationId) REFERENCES CommandRecord(operationId),
   CONSTRAINT changePayload CHECK (payloadVersion = 1 AND localRevision > 0 AND JSON_VALID(immutablePayload)),
   CONSTRAINT changeState CHECK (acknowledgmentState = 'pending'),
   INDEX changePending (datasetId, acknowledgmentState, createdAt)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
