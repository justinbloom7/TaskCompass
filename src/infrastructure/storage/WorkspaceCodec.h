#pragma once

#include "application/WorkspaceRepository.h"
#include <QJsonObject>
#include <QString>

namespace taskcompass {

QString entityTypeName(EntityKind kind);
QString commandTypeName(CommandOperation operation);
QJsonObject encodeEntity(const EntitySnapshot &entity);
EntitySnapshot decodeEntity(const QJsonObject &object, int payloadVersion = 2);
QString encodeReceiptSnapshot(const WorkspaceReceipt &receipt, bool before, int version = 2);

}
