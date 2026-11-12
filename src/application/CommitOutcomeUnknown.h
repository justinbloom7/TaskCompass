#pragma once

#include <stdexcept>

namespace taskcompass {

class CommitOutcomeUnknown : public std::runtime_error {
public:
   using std::runtime_error::runtime_error;
};

}
