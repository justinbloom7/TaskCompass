// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace taskcompass {

// Reads named constraints from MariaDB SHOW CREATE TABLE, not privilege-filtered
// TABLE_CONSTRAINTS. FK endpoints and the complete DDL fingerprint are checked
// separately by MigrationRunner. Unsupported constraint formatting fails closed.
class SchemaConstraints {
public:
   static QStringList namesFromDdl(const QString &ddl, const QString &kind);
};

}
