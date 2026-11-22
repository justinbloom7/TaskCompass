#include "WorkspaceCodec.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>
#include <type_traits>

namespace taskcompass {
QString entityTypeName(EntityKind kind) {
   switch (kind) {
      case EntityKind::TASK: return "task";
      case EntityKind::PILLAR: return "pillar";
      case EntityKind::PROJECT: return "project";
      case EntityKind::OBJECTIVE: return "objective";
   }
   throw std::invalid_argument("Unknown entity type.");
}
QString commandTypeName(CommandOperation operation) {
   switch (operation) {
      case CommandOperation::CREATE: return "create";
      case CommandOperation::EDIT: return "edit";
      case CommandOperation::COMPLETE: return "complete";
      case CommandOperation::REOPEN: return "reopen";
      case CommandOperation::MOVE: return "move";
      case CommandOperation::ARCHIVE: return "archive";
      case CommandOperation::RESTORE: return "restore";
      case CommandOperation::UNDO: return "undo";
      case CommandOperation::REDO: return "redo";
   }
   throw std::invalid_argument("Unknown command type.");
}
namespace {
QJsonValue optionalString(const std::optional<std::string> &value) {
   return value ? QJsonValue(QString::fromStdString(*value)) : QJsonValue(QJsonValue::Null);
}
QJsonValue optionalTime(const std::optional<Timestamp> &value) {
   return value ? QJsonValue(QString::number(*value)) : QJsonValue(QJsonValue::Null);
}
std::string text(const QJsonObject &object, const char *key) {
   if (!object[key].isString()) { throw std::invalid_argument("Missing/invalid snapshot text field."); }
   return object[key].toString().toStdString();
}
std::optional<std::string> optionalText(const QJsonObject &object, const char *key) {
   if (object[key].isNull() || object[key].isUndefined()) { return {}; }
   return text(object, key);
}
Timestamp number(const QJsonObject &object, const char *key) {
   bool valid = false;
   const auto value = object[key].toString().toLongLong(&valid);
   if (!object[key].isString() || !valid || QString::number(value) != object[key].toString()) {
      throw std::invalid_argument("Invalid decimal-string snapshot integer.");
   }
   return value;
}
std::optional<Timestamp> optionalNumber(const QJsonObject &object, const char *key) {
   if (object[key].isNull() || object[key].isUndefined()) { return {}; }
   return number(object, key);
}
}
QJsonObject encodeEntity(const EntitySnapshot &entity) {
   QJsonObject result;
   result["entityType"] = entityTypeName(entityKey(entity).kind);
   std::visit([&](const auto &value) {
      result["id"] = QString::fromStdString(value.id); result["datasetId"] = QString::fromStdString(value.datasetId);
      result["createdAt"] = QString::number(value.createdAt); result["updatedAt"] = QString::number(value.updatedAt);
      result["localRevision"] = QString::number(value.localRevision); result["deletedAt"] = optionalTime(value.deletedAt);
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Task>) {
         result["title"] = QString::fromStdString(value.title); result["notes"] = optionalString(value.notes);
         result["lifecycleStatus"] = value.status == TaskStatus::ACTIVE ? "active" : "completed";
         result["completedAt"] = optionalTime(value.completedAt);
         result["objectiveId"] = value.owner.kind == TaskOwnerKind::OBJECTIVE ? QJsonValue(QString::fromStdString(value.owner.id)) : QJsonValue(QJsonValue::Null);
         result["directProjectId"] = value.owner.kind == TaskOwnerKind::PROJECT ? QJsonValue(QString::fromStdString(value.owner.id)) : QJsonValue(QJsonValue::Null);
      } else {
         result["description"] = optionalString(value.description); result["archivedAt"] = optionalTime(value.archivedAt);
         if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Pillar>) {
            result["name"] = QString::fromStdString(value.name); result["parentPillarId"] = optionalString(value.parentPillarId);
         } else {
            result["title"] = QString::fromStdString(value.title); result["pillarId"] = optionalString(value.pillarId);
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Project>) { result["parentProjectId"] = optionalString(value.parentProjectId); }
            else { result["projectId"] = optionalString(value.projectId); }
         }
      }
   }, entity);
   return result;
}
EntitySnapshot decodeEntity(const QJsonObject &object, int payloadVersion) {
   if (payloadVersion != 1 && payloadVersion != 2) { throw std::invalid_argument("Unsupported snapshot payload version."); }
   const auto kind = payloadVersion == 1 ? QString("task") : object["entityType"].toString();
   EntitySnapshot result;
   if (kind == "pillar") { result = Pillar{}; }
   else if (kind == "project") { result = Project{}; }
   else if (kind == "objective") { result = Objective{}; }
   else if (kind != "task") { throw std::invalid_argument("Unknown snapshot entity type."); }
   std::visit([&](auto &value) {
      value.id = text(object, "id"); value.datasetId = text(object, "datasetId");
      value.createdAt = number(object, "createdAt"); value.updatedAt = number(object, "updatedAt");
      value.localRevision = number(object, "localRevision"); value.deletedAt = optionalNumber(object, "deletedAt");
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Task>) {
         value.title = text(object, "title"); value.notes = optionalText(object, "notes");
         const auto status = text(object, "lifecycleStatus");
         if (status != "active" && status != "completed") { throw std::invalid_argument("Unsupported task status."); }
         value.status = status == "active" ? TaskStatus::ACTIVE : TaskStatus::COMPLETED;
         value.completedAt = optionalNumber(object, "completedAt");
         if (payloadVersion == 2) {
            const auto objective = optionalText(object, "objectiveId"), project = optionalText(object, "directProjectId");
            if (objective && project) { throw std::invalid_argument("Task has contradictory owners."); }
            if (objective) { value.owner = {TaskOwnerKind::OBJECTIVE, *objective}; }
            if (project) { value.owner = {TaskOwnerKind::PROJECT, *project}; }
         }
         validateTask(value);
      } else {
         value.description = optionalText(object, "description"); value.archivedAt = optionalNumber(object, "archivedAt");
         if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Pillar>) {
            value.name = text(object, "name"); value.parentPillarId = optionalText(object, "parentPillarId");
         } else {
            value.title = text(object, "title"); value.pillarId = optionalText(object, "pillarId");
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Project>) { value.parentProjectId = optionalText(object, "parentProjectId"); }
            else { value.projectId = optionalText(object, "projectId"); }
         }
         validateOrganization(OrganizationEntity(value));
      }
   }, result);
   return result;
}
QString encodeReceiptSnapshot(const WorkspaceReceipt &receipt, bool before) {
   auto object = encodeEntity(before ? *receipt.before : receipt.after);
   object["changedFields"] = static_cast<int>(receipt.changedFields);
   QJsonArray dependencies;
   for (const auto &item : receipt.dependencies) {
      dependencies.append(QJsonObject{{"entityType", entityTypeName(item.key.kind)}, {"id", QString::fromStdString(item.key.id)},
                                     {"revision", QString::number(item.revision)}});
   }
   object["dependencies"] = dependencies;
   return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

}
