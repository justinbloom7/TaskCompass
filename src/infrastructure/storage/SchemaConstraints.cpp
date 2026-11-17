// SPDX-License-Identifier: GPL-3.0-or-later
#include "SchemaConstraints.h"

#include <QRegularExpression>
#include <stdexcept>

namespace taskcompass {

QStringList SchemaConstraints::namesFromDdl(const QString &ddl, const QString &kind) {
   if ((kind != "CHECK" && kind != "FOREIGN KEY")
       || !ddl.startsWith("CREATE TABLE ") || !ddl.contains("\n)")) {
      throw std::runtime_error("Unsupported SHOW CREATE TABLE constraint definition.");
   }
   static const QRegularExpression CONSTRAINT_PATTERN(
      R"(^CONSTRAINT `((?:``|[^`])+)` (CHECK|FOREIGN KEY) \(.+\),?$)");
   QStringList result;
   QStringList allNames;
   for (const auto &line : ddl.split('\n')) {
      const auto definition = line.trimmed();
      if (!definition.startsWith("CONSTRAINT", Qt::CaseInsensitive)) { continue; }
      const auto match = CONSTRAINT_PATTERN.match(definition);
      if (!match.hasMatch()) {
         throw std::runtime_error("Unsupported SHOW CREATE TABLE constraint formatting; operator review required.");
      }
      auto name = match.captured(1);
      name.replace("``", "`");
      if (allNames.contains(name)) {
         throw std::runtime_error("Duplicate SHOW CREATE TABLE constraint name.");
      }
      allNames.append(name);
      if (match.captured(2) == kind) { result.append(name); }
   }
   result.sort();
   return result;
}

}
