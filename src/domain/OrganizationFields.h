#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace taskcompass {

struct OrganizationFields {
   std::string id;
   std::string datasetId;
   std::optional<std::string> description;
   std::optional<std::int64_t> archivedAt;
   std::int64_t createdAt = 0;
   std::int64_t updatedAt = 0;
   std::optional<std::int64_t> deletedAt;
   std::int64_t localRevision = 0;
   bool operator==(const OrganizationFields &) const = default;
};

}
