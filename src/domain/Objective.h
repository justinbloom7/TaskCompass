#pragma once

#include "OrganizationFields.h"

namespace taskcompass {

struct Objective : OrganizationFields {
   std::string title;
   std::optional<std::string> pillarId;
   std::optional<std::string> projectId;
   bool operator==(const Objective &) const = default;
};

}
