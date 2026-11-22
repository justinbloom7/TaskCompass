#pragma once

#include "OrganizationFields.h"

namespace taskcompass {

struct Pillar : OrganizationFields {
   std::string name;
   std::optional<std::string> parentPillarId;
   bool operator==(const Pillar &) const = default;
};

}
