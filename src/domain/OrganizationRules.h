#pragma once

#include "Pillar.h"
#include "Project.h"
#include "Objective.h"
#include "Task.h"
#include <compare>
#include <map>
#include <variant>
#include <vector>

namespace taskcompass {

enum class OrganizationKind { PILLAR, PROJECT, OBJECTIVE };
struct OrganizationKey {
   OrganizationKind kind = OrganizationKind::PILLAR;
   std::string id;
   auto operator<=>(const OrganizationKey &) const = default;
};
using OrganizationEntity = std::variant<Pillar, Project, Objective>;

OrganizationFields &organizationFields(OrganizationEntity &entity);
const OrganizationFields &organizationFields(const OrganizationEntity &entity);
OrganizationKey organizationKey(const OrganizationEntity &entity);
std::string organizationTitle(const OrganizationEntity &entity);
void setOrganizationTitle(OrganizationEntity &entity, const std::string &title);
std::optional<OrganizationKey> organizationParent(const OrganizationEntity &entity);
void setOrganizationParent(OrganizationEntity &entity, const std::optional<OrganizationKey> &parent);
void validateOrganization(const OrganizationEntity &entity);

class OrganizationRules {
public:
   OrganizationRules(std::string datasetId, const std::vector<OrganizationEntity> &entities);
   const OrganizationEntity &find(const OrganizationKey &key) const;
   std::vector<OrganizationKey> path(const OrganizationKey &key) const;
   bool effectivelyArchived(const OrganizationKey &key) const;
   bool taskArchived(const Task &task) const;
   void requireAvailableParent(const OrganizationEntity &entity, bool allowArchived = false) const;
   void requireTaskOwner(const Task &task, bool allowArchived = false) const;
   void requireRemovable(const OrganizationKey &key, const std::vector<Task> &tasks) const;

private:
   std::string datasetId;
   std::map<OrganizationKey, OrganizationEntity> entities;
};

}
