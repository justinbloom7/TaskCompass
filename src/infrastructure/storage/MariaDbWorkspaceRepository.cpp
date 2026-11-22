#include "MariaDbWorkspaceRepository.h"
#include "WorkspaceCodec.h"
#include "MutationLease.h"
#include "MigrationRunner.h"
#include <QSqlQuery>
#include <QVariant>
#include <QJsonDocument>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace taskcompass {
namespace {
void require(bool condition, const char *message) { if (!condition) { throw std::runtime_error(message); } }
QString table(EntityKind kind) {
   switch (kind) {
      case EntityKind::TASK: return "Task";
      case EntityKind::PILLAR: return "Pillar";
      case EntityKind::PROJECT: return "Project";
      case EntityKind::OBJECTIVE: return "Objective";
   }
   throw std::invalid_argument("Invalid entity kind.");
}
QStringList columns(EntityKind kind) {
   QStringList result{"id", "datasetId", "createdAt", "updatedAt", "deletedAt", "localRevision"};
   switch (kind) {
      case EntityKind::TASK: result.append({"title", "notes", "lifecycleStatus", "completedAt", "objectiveId", "directProjectId"}); break;
      case EntityKind::PILLAR: result.append({"name", "description", "archivedAt", "parentPillarId"}); break;
      case EntityKind::PROJECT: result.append({"title", "description", "archivedAt", "pillarId", "parentProjectId"}); break;
      case EntityKind::OBJECTIVE: result.append({"title", "description", "archivedAt", "pillarId", "projectId"}); break;
   }
   return result;
}
QVariant value(const QJsonObject &object, const QString &column) {
   const bool numeric = column.endsWith("At") || column == "localRevision";
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
WorkspaceSnapshot MariaDbWorkspaceRepository::load() {
   WorkspaceSnapshot snapshot;
   for (const auto kind : {EntityKind::TASK, EntityKind::PILLAR, EntityKind::PROJECT, EntityKind::OBJECTIVE}) {
      const auto names = columns(kind);
      QSqlQuery query(database);
      query.prepare("SELECT " + names.join(',') + " FROM " + table(kind) + " WHERE datasetId=? ORDER BY id");
      query.addBindValue(QString::fromStdString(datasetId));
      require(query.exec(), "Cannot read workspace; check version/runtime SELECT grants and reload.");
      while (query.next()) {
         QJsonObject object{{"entityType", entityTypeName(kind)}};
         for (int index = 0; index < names.size(); ++index) {
            object[names[index]] = query.value(index).isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(query.value(index).toString());
         }
         auto entity = decodeEntity(object);
         if (const auto *task = std::get_if<Task>(&entity)) { snapshot.tasks.push_back(*task); }
         else { snapshot.organizations.push_back(asOrganization(entity)); }
      }
   }
   OrganizationRules graph(datasetId, snapshot.organizations);
   for (const auto &entity : snapshot.organizations) {
      if (!organizationFields(entity).deletedAt) { graph.requireAvailableParent(entity, true); }
   }
   for (const auto &task : snapshot.tasks) { if (!task.deletedAt) { graph.requireTaskOwner(task, true); } }
   return snapshot;
}
WorkspaceSnapshot MariaDbWorkspaceRepository::read() {
   MutationLease lease(database);
   require(MigrationRunner(database).verify(2) == datasetId, "Workspace dataset differs from configured session.");
   require(database.transaction(), "Cannot start coherent workspace read.");
   try {
      auto snapshot = load();
      require(database.commit(), "Workspace read failed; reload.");
      return snapshot;
   } catch (...) { database.rollback(); throw; }
}
WorkspaceReceipt MariaDbWorkspaceRepository::transact(const Prepare &prepare) {
   MutationLease lease(database);
   require(MigrationRunner(database).verify(2) == datasetId, "Workspace dataset differs from configured session.");
   require(database.transaction(), "Cannot start workspace transaction; no writes submitted.");
   try {
      const auto snapshot = load();
      const auto receipt = prepare(snapshot);
      write(receipt, snapshot);
      if (!database.commit()) { throw CommitOutcomeUnknown("Workspace commit outcome unknown. Clear session history and reload before retrying."); }
      return receipt;
   } catch (...) { database.rollback(); throw; }
}
void MariaDbWorkspaceRepository::write(const WorkspaceReceipt &receipt, const WorkspaceSnapshot &current) {
   require(entityDataset(receipt.after) == datasetId && isUuid(receipt.id) && isUuid(receipt.operationId), "Invalid receipt identity/scope.");
   const auto key = entityKey(receipt.after);
   std::optional<EntitySnapshot> original;
   if (key.kind == EntityKind::TASK) {
      validateTask(std::get<Task>(receipt.after));
      for (const auto &item : current.tasks) { if (item.id == key.id) { original = item; break; } }
   } else {
      validateOrganization(asOrganization(receipt.after));
      for (const auto &item : current.organizations) { if (entityKey(asSnapshot(item)) == key) { original = asSnapshot(item); break; } }
   }
   require(original == receipt.before, "Receipt before state does not match authoritative transaction state.");
   require(!original || entityRevision(*original) < std::numeric_limits<std::int64_t>::max(), "Record revision exhausted.");
   require(entityRevision(receipt.after) == (original ? entityRevision(*original) + 1 : 1), "Invalid receipt revision transition.");
   const auto object = encodeEntity(receipt.after);
   if (original) {
      const auto old = encodeEntity(*original);
      require(old["createdAt"] == object["createdAt"] && old["datasetId"] == object["datasetId"] && entityKey(*original) == key,
              "Immutable identity/creation fields changed.");
   }
   auto names = columns(key.kind);
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
   if (afterWrite) { afterWrite(0); }
   const auto afterJson = encodeReceiptSnapshot(receipt, false);
   const auto beforeJson = receipt.before ? QVariant(encodeReceiptSnapshot(receipt, true)) : QVariant(QMetaType::fromType<QString>());
   const auto recordedAt = value(object, "updatedAt");
   auto bindTargets = [&](QSqlQuery &statement) {
      for (int kind = 0; kind < 4; ++kind) {
         statement.addBindValue(kind == static_cast<int>(key.kind) ? QVariant(QString::fromStdString(key.id)) : QVariant(QMetaType::fromType<QString>()));
      }
   };
   query.prepare("INSERT INTO CommandRecord (id,operationId,datasetId,taskId,pillarId,projectId,objectiveId,commandType,payloadVersion,recordedAt,beforeSnapshot,afterSnapshot,resultingRevision,compensatesCommandId,reappliesCommandId) VALUES (?,?,?,?,?,?,?,?,2,?,?,?,?,?,?)");
   query.addBindValue(QString::fromStdString(receipt.id)); query.addBindValue(QString::fromStdString(receipt.operationId));
   query.addBindValue(QString::fromStdString(datasetId)); bindTargets(query); query.addBindValue(commandTypeName(receipt.operation));
   query.addBindValue(recordedAt); query.addBindValue(beforeJson); query.addBindValue(afterJson);
   query.addBindValue(QVariant::fromValue<qlonglong>(entityRevision(receipt.after)));
   query.addBindValue(receipt.compensatesCommandId ? QVariant(QString::fromStdString(*receipt.compensatesCommandId)) : QVariant(QMetaType::fromType<QString>()));
   query.addBindValue(receipt.reappliesCommandId ? QVariant(QString::fromStdString(*receipt.reappliesCommandId)) : QVariant(QMetaType::fromType<QString>()));
   require(query.exec(), "Receipt insert failed; workspace transaction rolled back.");
   if (afterWrite) { afterWrite(1); }
   query.prepare("INSERT INTO LocalChange (id,operationId,datasetId,taskId,pillarId,projectId,objectiveId,localRevision,payloadVersion,immutablePayload,createdAt,acknowledgmentState) VALUES (?,?,?,?,?,?,?,?,2,?,?,'pending')");
   query.addBindValue(QString::fromStdString(receipt.id)); query.addBindValue(QString::fromStdString(receipt.operationId));
   query.addBindValue(QString::fromStdString(datasetId)); bindTargets(query);
   query.addBindValue(QVariant::fromValue<qlonglong>(entityRevision(receipt.after))); query.addBindValue(afterJson); query.addBindValue(recordedAt);
   require(query.exec(), "Change insert failed; workspace/receipt transaction rolled back.");
   if (afterWrite) { afterWrite(2); }
}

}
