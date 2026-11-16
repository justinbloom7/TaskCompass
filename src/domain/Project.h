#pragma once

#include "OrganizationFields.h"

namespace taskcompass {

struct Project : OrganizationFields {
   std::string title;
   std::optional<std::string> pillarId;
   std::optional<std::string> parentProjectId;
   bool operator==(const Project &) const = default;
};

}
