#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace taskcompass {

class CommitOutcomeUnknown : public std::runtime_error {
public:
   explicit CommitOutcomeUnknown(const std::string &message, std::string operationId = {}, std::string datasetId = {})
      : std::runtime_error(message), operationId(std::move(operationId)), datasetId(std::move(datasetId)) {}
   const std::string operationId;
   const std::string datasetId;
};

}
