#include "MariaDbWorkspaceRepository.h"
#include "WorkspaceCodec.h"
#include "MutationLease.h"
#include "MigrationRunner.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <set>

namespace taskcompass {
namespace {
void require(bool condition, const char *message) { if (!condition) { throw std::runtime_error(message); } }
QString table(EntityKind kind) {
   switch (kind) {
      case EntityKind::TASK: return "Task";
      case EntityKind::PILLAR: return "Pillar";
      case EntityKind::PROJECT: return "Project";
      case EntityKind::OBJECTIVE: return "Objective";
      case EntityKind::CONTEXT: return "Context";
      case EntityKind::TASK_CONTEXT: return "TaskContext";
   }
   throw std::invalid_argument("Invalid entity kind.");
}
QStringList columns(EntityKind kind, int version) {
   QStringList result{"id", "datasetId", "createdAt", "updatedAt", "deletedAt", "localRevision"};
   switch (kind) {
      case EntityKind::TASK:
         result.append({"title", "notes", "lifecycleStatus", "completedAt", "objectiveId", "directProjectId"});
         if (version == 3) { result.append({"priority", "manualOrderKey"}); }
         break;
      case EntityKind::PILLAR: result.append({"name", "description", "archivedAt", "parentPillarId"}); break;
      case EntityKind::PROJECT: result.append({"title", "description", "archivedAt", "pillarId", "parentProjectId"}); break;
      case EntityKind::OBJECTIVE: result.append({"title", "description", "archivedAt", "pillarId", "projectId"}); break;
      case EntityKind::CONTEXT: result.append({"name", "sortKey", "archivedAt"}); break;
      case EntityKind::TASK_CONTEXT: result.append({"taskId", "contextId"}); break;
   }
   return result;
}
QVariant value(const QJsonObject &object, const QString &column) {
   const bool numeric = column.endsWith("At") || column == "localRevision" || column == "priority";
   if (column == "manualOrderKey" || column == "sortKey") { return object[column].toString().toLatin1(); }
   if (object[column].isNull()) {
      return numeric ? QVariant(QMetaType::fromType<qlonglong>()) : QVariant(QMetaType::fromType<QString>());
   }
   return numeric ? QVariant::fromValue<qlonglong>(object[column].toString().toLongLong()) : QVariant(object[column].toString());
}
}
MariaDbWorkspaceRepository::MariaDbWorkspaceRepository(QSqlDatabase database, std::string datasetId,
                                                       std::function<void(int)> afterWrite)
   : database(database), datasetId(std::move(datasetId)), afterWrite(std::move(afterWrite)) {
   require(isUuid(this->datasetId), "Invalid workspace dataset.");
}
WorkspaceSnapshot MariaDbWorkspaceRepository::load(int version) {
   WorkspaceSnapshot snapshot;
   snapshot.schemaVersion = version;
   const auto count = version == 3 ? 6 : 4;
   for (int index = 0; index < count; ++index) {
      const auto kind = static_cast<EntityKind>(index);
      const auto names = columns(kind, version);
      QSqlQuery query(database);
      query.prepare("SELECT " + names.join(',') + " FROM " + table(kind) + " WHERE datasetId=? ORDER BY id");
      query.addBindValue(QString::fromStdString(datasetId));
      require(query.exec(), "Cannot read workspace; check version/runtime SELECT grants and reload.");
      while (query.next()) {
         QJsonObject object{{"entityType", entityTypeName(kind)}};
         for (int index = 0; index < names.size(); ++index) {
            object[names[index]] = query.value(index).isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(query.value(index).toString());
         }
         auto entity = decodeEntity(object, version);
         if (const auto *task = std::get_if<Task>(&entity)) { snapshot.tasks.push_back(*task); }
         else if (const auto *context = std::get_if<Context>(&entity)) { snapshot.contexts.push_back(*context); }
         else if (const auto *link = std::get_if<TaskContext>(&entity)) { snapshot.taskContexts.push_back(*link); }
         else { snapshot.organizations.push_back(asOrganization(entity)); }
      }
      require(!query.lastError().isValid(), "Workspace read was interrupted.");
   }
   OrganizationRules graph(datasetId, snapshot.organizations);
   for (const auto &entity : snapshot.organizations) {
      if (!organizationFields(entity).deletedAt) { graph.requireAvailableParent(entity, true); }
   }
   for (const auto &task : snapshot.tasks) { if (!task.deletedAt) { graph.requireTaskOwner(task, true); } }
   validateContextState(snapshot, datasetId);
   return snapshot;
}
WorkspaceSnapshot MariaDbWorkspaceRepository::read() {
   MutationLease lease(database);
   MigrationRunner runner(database);
   require(runner.verify() == datasetId && runner.verifiedVersion() >= 2, "Workspace schema or dataset differs from configured session.");
   require(database.transaction(), "Cannot start coherent workspace read.");
   try {
      auto snapshot = load(runner.verifiedVersion());
      require(database.commit(), "Workspace read failed; reload.");
      return snapshot;
   } catch (...) { database.rollback(); throw; }
}
bool MariaDbWorkspaceRepository::operationCommitted(const std::string &operationId, const std::string &expectedDataset) {
   require(isUuid(operationId) && datasetId == expectedDataset, "Uncertain operation belongs to a different dataset.");
   MutationLease lease(database);
   MigrationRunner runner(database);
   require(runner.verify(3) == datasetId, "Workspace dataset differs while resolving a commit.");
   QSqlQuery query(database);
   query.prepare("SELECT (SELECT COUNT(*) FROM CommandRecord WHERE datasetId=? AND operationId=?),"
                 " (SELECT COUNT(*) FROM LocalChange WHERE datasetId=? AND operationId=?),"
                 " (SELECT COUNT(*) FROM CommandRecord audit JOIN LocalChange outbox ON audit.operationId=outbox.operationId"
                 " WHERE audit.datasetId=? AND audit.operationId=? AND audit.datasetId=outbox.datasetId"
                 " AND audit.id=outbox.id AND audit.payloadVersion=3 AND outbox.payloadVersion=3"
                 " AND audit.resultingRevision=outbox.localRevision"
                 " AND CAST(audit.afterSnapshot AS BINARY)=CAST(outbox.immutablePayload AS BINARY))");
   for (int i = 0; i < 3; ++i) {
      query.addBindValue(QString::fromStdString(datasetId));
      query.addBindValue(QString::fromStdString(operationId));
   }
   require(query.exec() && query.next(), "Cannot resolve the operation after a lost commit acknowledgment.");
   const auto receipts = query.value(0).toInt();
   const auto changes = query.value(1).toInt();
   require(receipts == changes && receipts <= 1 && query.value(2).toInt() == receipts, "Audit and outbox disagree. Preserve evidence and request review.");
   return receipts == 1;
}

WorkspaceReceipt MariaDbWorkspaceRepository::transact(const Prepare &prepare) {
   MutationLease lease(database);
   MigrationRunner runner(database);
   require(runner.verify() == datasetId && runner.verifiedVersion() >= 2, "Workspace schema or dataset differs from configured session.");
   require(database.transaction(), "Cannot start workspace transaction; no writes submitted.");
   try {
      const auto snapshot = load(runner.verifiedVersion());
      const auto receipt = prepare(snapshot);
      write(receipt, snapshot);
      if (!database.commit()) {
         throw CommitOutcomeUnknown("Workspace commit outcome unknown. Reload before attempting a new command.",
                                    snapshot.schemaVersion == 3 ? receipt.operationId : std::string{}, datasetId);
      }
      return receipt;
   } catch (...) { database.rollback(); throw; }
}
void MariaDbWorkspaceRepository::writePatch(const WorkspacePatch &patch, const WorkspaceSnapshot &current) {
   require(entityDataset(patch.after) == datasetId, "Invalid patch dataset.");
   const auto key = entityKey(patch.after);
   const auto original = findEntity(current, key);
   require(original == patch.before, "Patch before state does not match authoritative transaction state.");
   require(!original || entityRevision(*original) < std::numeric_limits<std::int64_t>::max(), "Record revision exhausted.");
   require(entityRevision(patch.after) == (original ? entityRevision(*original) + 1 : 1), "Invalid patch revision transition.");
   const auto object = encodeEntity(patch.after);
   static_cast<void>(decodeEntity(object, current.schemaVersion));
   if (original) {
      const auto old = encodeEntity(*original);
      require(old["createdAt"] == object["createdAt"] && old["datasetId"] == object["datasetId"],
              "Immutable identity/creation fields changed.");
      if (key.kind == EntityKind::TASK_CONTEXT) {
         require(old["taskId"] == object["taskId"] && old["contextId"] == object["contextId"],
                 "Immutable assignment endpoints changed.");
      }
   }
   auto names = columns(key.kind, current.schemaVersion);
   QSqlQuery query(database);
   if (original) {
      names.removeAll("id"); names.removeAll("datasetId"); names.removeAll("createdAt");
      QStringList assignments;
      for (const auto &name : names) { assignments.append(name + "=?"); }
      query.prepare("UPDATE " + table(key.kind) + " SET " + assignments.join(',') + " WHERE datasetId=? AND id=? AND localRevision=?");
   } else {
      QStringList placeholders;
      for (qsizetype index = 0; index < names.size(); ++index) { placeholders.append("?"); }
      query.prepare("INSERT INTO " + table(key.kind) + " (" + names.join(',') + ") VALUES (" + placeholders.join(',') + ")");
   }
   for (const auto &name : names) { query.addBindValue(value(object, name)); }
   if (original) {
      query.addBindValue(QString::fromStdString(datasetId)); query.addBindValue(QString::fromStdString(key.id));
      query.addBindValue(QVariant::fromValue<qlonglong>(entityRevision(*original)));
   }
   require(query.exec() && query.numRowsAffected() == 1, "Workspace write/revision conflict; transaction rolled back. Reload.");
}

void MariaDbWorkspaceRepository::write(const WorkspaceReceipt &receipt, const WorkspaceSnapshot &current) {
   require(isUuid(receipt.id) && isUuid(receipt.operationId), "Invalid receipt identity.");
   const auto key = entityKey(receipt.after);
   const int version = current.schemaVersion;
   require(key.kind != EntityKind::TASK_CONTEXT, "A link cannot be a command root.");
   require(version == 3 || (receipt.patches.empty() && receipt.contextGuards.empty()), "Multi-record receipts require schema 3.");
   auto next = current;
   putEntity(next, receipt.after);
   std::set<EntityKey> seen{key};
   for (const auto &patch : receipt.patches) {
      require(seen.insert(entityKey(patch.after)).second, "Duplicate command patch.");
      putEntity(next, patch.after);
   }
   validateContextState(next, datasetId);
   for (const auto &guard : receipt.contextGuards) {
      require(contextWitness(current, guard) == guard.before && contextWitness(next, guard) == guard.after,
              "Context topology changed; transaction rolled back.");
   }
   writePatch({receipt.before, receipt.after, receipt.changedFields}, current);
   if (afterWrite) { afterWrite(0); }
   const auto afterJson = encodeReceiptSnapshot(receipt, false, version);
   const auto beforeJson = receipt.before ? QVariant(encodeReceiptSnapshot(receipt, true, version)) : QVariant(QMetaType::fromType<QString>());
   const auto recordedAt = value(encodeEntity(receipt.after), "updatedAt");
   const auto bindTargets = [&](QSqlQuery &statement) {
      for (int kind = 0; kind < (version == 3 ? 5 : 4); ++kind) {
         statement.addBindValue(kind == static_cast<int>(key.kind) ? QVariant(QString::fromStdString(key.id)) : QVariant(QMetaType::fromType<QString>()));
      }
   };
   QSqlQuery query(database);
   const QString contextColumn = version == 3 ? ",contextId" : "";
   const QString contextBind = version == 3 ? ",?" : "";
   query.prepare("INSERT INTO CommandRecord (id,operationId,datasetId,taskId,pillarId,projectId,objectiveId" + contextColumn
                 + ",commandType,payloadVersion,recordedAt,beforeSnapshot,afterSnapshot,resultingRevision,compensatesCommandId,reappliesCommandId)"
                   " VALUES (?,?,?,?,?,?,?" + contextBind + ",?,?,?,?,?,?,?,?)");
   query.addBindValue(QString::fromStdString(receipt.id));
   query.addBindValue(QString::fromStdString(receipt.operationId));
   query.addBindValue(QString::fromStdString(datasetId));
   bindTargets(query);
   query.addBindValue(commandTypeName(receipt.operation));
   query.addBindValue(version);
   query.addBindValue(recordedAt);
   query.addBindValue(beforeJson);
   query.addBindValue(afterJson);
   query.addBindValue(QVariant::fromValue<qlonglong>(entityRevision(receipt.after)));
   query.addBindValue(receipt.compensatesCommandId ? QVariant(QString::fromStdString(*receipt.compensatesCommandId)) : QVariant(QMetaType::fromType<QString>()));
   query.addBindValue(receipt.reappliesCommandId ? QVariant(QString::fromStdString(*receipt.reappliesCommandId)) : QVariant(QMetaType::fromType<QString>()));
   require(query.exec(), "Receipt insert failed; transaction rolled back.");
   if (afterWrite) { afterWrite(1); }
   int step = 2;
   auto patches = receipt.patches;
   const auto order = [](const auto &patch) {
      const auto *link = std::get_if<TaskContext>(&patch.after);
      return std::pair{link ? (link->deletedAt ? 0 : 2) : 1, entityKey(patch.after)};
   };
   std::ranges::sort(patches, {}, order);
   for (const auto &patch : patches) {
      writePatch(patch, current);
      if (afterWrite) { afterWrite(step++); }
   }
   query.prepare("INSERT INTO LocalChange (id,operationId,datasetId,taskId,pillarId,projectId,objectiveId" + contextColumn
                 + ",localRevision,payloadVersion,immutablePayload,createdAt,acknowledgmentState) VALUES (?,?,?,?,?,?,?"
                 + contextBind + ",?,?,?,?,'pending')");
   query.addBindValue(QString::fromStdString(receipt.id));
   query.addBindValue(QString::fromStdString(receipt.operationId));
   query.addBindValue(QString::fromStdString(datasetId));
   bindTargets(query);
   query.addBindValue(QVariant::fromValue<qlonglong>(entityRevision(receipt.after)));
   query.addBindValue(version);
   query.addBindValue(afterJson);
   query.addBindValue(recordedAt);
   require(query.exec(), "Change insert failed; transaction rolled back.");
   if (afterWrite) { afterWrite(step); }
}

}
