#include "MariaDbTaskRepository.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QVariant>
#include <stdexcept>
#include <utility>

namespace taskcompass {
namespace {
const QString COLUMNS = "id,datasetId,title,notes,lifecycleStatus,createdAt,updatedAt,completedAt,deletedAt,localRevision";
QString string(const std::string &value) { return QString::fromStdString(value); }
QVariant nullable(const std::optional<Timestamp> &value) {
   return value ? QVariant::fromValue<qlonglong>(*value) : QVariant(QMetaType::fromType<qlonglong>());
}
QVariant nullable(const std::optional<std::string> &value) {
   return value ? QVariant(string(*value)) : QVariant(QMetaType::fromType<QString>());
}

Task readTask(const QSqlQuery &query) {
   Task task;
   task.id = query.value(0).toString().toStdString();
   task.datasetId = query.value(1).toString().toStdString();
   task.title = query.value(2).toString().toStdString();
   if (!query.value(3).isNull()) {
      task.notes = query.value(3).toString().toStdString();
   }
   const auto status = query.value(4).toString();
   if (status != "active" && status != "completed") {
      throw std::runtime_error("Unsupported persisted task status.");
   }
   task.status = status == "active" ? TaskStatus::ACTIVE : TaskStatus::COMPLETED;
   task.createdAt = query.value(5).toLongLong();
   task.updatedAt = query.value(6).toLongLong();
   if (!query.value(7).isNull()) {
      task.completedAt = query.value(7).toLongLong();
   }
   if (!query.value(8).isNull()) {
      task.deletedAt = query.value(8).toLongLong();
   }
   task.localRevision = query.value(9).toLongLong();
   validateTask(task);
   return task;
}

QString snapshot(const Task &task) {
   // Integers are decimal strings: future JSON consumers must not lose 64-bit precision.
   QJsonObject object{{"id", string(task.id)}, {"datasetId", string(task.datasetId)},
                      {"title", string(task.title)},
                      {"notes", task.notes ? QJsonValue(string(*task.notes)) : QJsonValue(QJsonValue::Null)},
                      {"lifecycleStatus", task.status == TaskStatus::ACTIVE ? "active" : "completed"},
                      {"createdAt", QString::number(task.createdAt)}, {"updatedAt", QString::number(task.updatedAt)},
                      {"localRevision", QString::number(task.localRevision)},
                      {"completedAt", task.completedAt ? QJsonValue(QString::number(*task.completedAt)) : QJsonValue(QJsonValue::Null)},
                      {"deletedAt", task.deletedAt ? QJsonValue(QString::number(*task.deletedAt)) : QJsonValue(QJsonValue::Null)}};
   return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString operationName(TaskOperation operation) {
   switch (operation) {
      case TaskOperation::CREATE: return "create";
      case TaskOperation::EDIT: return "edit";
      case TaskOperation::COMPLETE: return "complete";
      case TaskOperation::REOPEN: return "reopen";
      case TaskOperation::UNDO: return "undo";
      case TaskOperation::REDO: return "redo";
   }
   throw std::invalid_argument("Invalid command operation.");
}

void require(bool condition, const char *message) {
   if (!condition) {
      throw std::runtime_error(message);
   }
}
}

MariaDbTaskRepository::MariaDbTaskRepository(QSqlDatabase database, std::string datasetId,
                                             std::function<void()> afterTaskWrite)
   : database(database), datasetId(std::move(datasetId)), afterTaskWrite(std::move(afterTaskWrite)) {}

std::vector<Task> MariaDbTaskRepository::list() {
   QSqlQuery query(database);
   query.prepare("SELECT " + COLUMNS + " FROM Task WHERE datasetId=? AND deletedAt IS NULL ORDER BY id");
   query.addBindValue(string(datasetId));
   require(query.exec(), "Cannot load tasks; check runtime SELECT privileges and database availability.");
   std::vector<Task> result;
   while (query.next()) {
      result.push_back(readTask(query));
   }
   return result;
}

std::optional<Task> MariaDbTaskRepository::find(const std::string &id) {
   QSqlQuery query(database);
   query.prepare("SELECT " + COLUMNS + " FROM Task WHERE datasetId=? AND id=?");
   query.addBindValue(string(datasetId));
   query.addBindValue(string(id));
   require(query.exec(), "Cannot read task; check database availability.");
   if (!query.next()) {
      return {};
   }
   return readTask(query);
}

void MariaDbTaskRepository::commit(const CommandReceipt &receipt) {
   const auto &task = receipt.after;
   validateTask(task);
   require(task.datasetId == datasetId && isUuid(receipt.id) && isUuid(receipt.operationId), "Invalid command scope or identity.");
   require((!receipt.before && task.localRevision == 1)
           || (receipt.before && receipt.before->id == task.id && receipt.before->datasetId == datasetId
               && receipt.before->localRevision == task.localRevision - 1), "Invalid revision transition.");
   require(database.transaction(), "Cannot start task transaction; no changes were submitted.");
   try {
      QSqlQuery query(database);
      if (receipt.before) {
         query.prepare("UPDATE Task SET title=?,notes=?,lifecycleStatus=?,createdAt=?,updatedAt=?,completedAt=?,deletedAt=?,localRevision=? WHERE datasetId=? AND id=? AND localRevision=?");
      } else {
         query.prepare("INSERT INTO Task (title,notes,lifecycleStatus,createdAt,updatedAt,completedAt,deletedAt,localRevision,datasetId,id) VALUES (?,?,?,?,?,?,?,?,?,?)");
      }
      query.addBindValue(string(task.title));
      query.addBindValue(nullable(task.notes));
      query.addBindValue(task.status == TaskStatus::ACTIVE ? "active" : "completed");
      query.addBindValue(QVariant::fromValue<qlonglong>(task.createdAt));
      query.addBindValue(QVariant::fromValue<qlonglong>(task.updatedAt));
      query.addBindValue(nullable(task.completedAt));
      query.addBindValue(nullable(task.deletedAt));
      query.addBindValue(QVariant::fromValue<qlonglong>(task.localRevision));
      query.addBindValue(string(datasetId));
      query.addBindValue(string(task.id));
      if (receipt.before) {
         query.addBindValue(QVariant::fromValue<qlonglong>(receipt.before->localRevision));
      }
      require(query.exec(), "Task write failed; transaction will be rolled back. Check grants and constraints.");
      require(query.numRowsAffected() == 1, "Task revision changed; transaction rolled back. Refresh before retrying.");
      if (afterTaskWrite) {
         afterTaskWrite();
      }
      const auto afterJson = snapshot(task);
      QSqlQuery audit(database);
      audit.prepare("INSERT INTO CommandRecord (id,operationId,datasetId,taskId,commandType,payloadVersion,recordedAt,beforeSnapshot,afterSnapshot,resultingRevision,compensatesCommandId,reappliesCommandId) VALUES (?,?,?,?,?,1,?,?,?,?,?,?)");
      audit.addBindValue(string(receipt.id));
      audit.addBindValue(string(receipt.operationId));
      audit.addBindValue(string(datasetId));
      audit.addBindValue(string(task.id));
      audit.addBindValue(operationName(receipt.operation));
      audit.addBindValue(QVariant::fromValue<qlonglong>(task.updatedAt));
      audit.addBindValue(receipt.before ? QVariant(snapshot(*receipt.before)) : QVariant(QMetaType::fromType<QString>()));
      audit.addBindValue(afterJson);
      audit.addBindValue(QVariant::fromValue<qlonglong>(task.localRevision));
      audit.addBindValue(nullable(receipt.compensatesCommandId));
      audit.addBindValue(nullable(receipt.reappliesCommandId));
      require(audit.exec(), "Command receipt write failed; task transaction rolled back.");
      QSqlQuery change(database);
      change.prepare("INSERT INTO LocalChange (id,operationId,datasetId,taskId,localRevision,payloadVersion,immutablePayload,createdAt,acknowledgmentState) VALUES (?,?,?,?,?,1,?,?,'pending')");
      change.addBindValue(string(receipt.id));
      change.addBindValue(string(receipt.operationId));
      change.addBindValue(string(datasetId));
      change.addBindValue(string(task.id));
      change.addBindValue(QVariant::fromValue<qlonglong>(task.localRevision));
      change.addBindValue(afterJson);
      change.addBindValue(QVariant::fromValue<qlonglong>(task.updatedAt));
      require(change.exec(), "Dormant change write failed; task and receipt transaction rolled back.");
      if (!database.commit()) {
         throw CommitOutcomeUnknown("Commit could not be confirmed. Session history must be cleared; reload and inspect before retrying.");
      }
   } catch (...) {
      database.rollback();
      throw;
   }
}

}
