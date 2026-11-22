#include "OrganizationRules.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace taskcompass {

OrganizationFields &organizationFields(OrganizationEntity &entity) {
   return std::visit([](auto &value) -> OrganizationFields & { return value; }, entity);
}
const OrganizationFields &organizationFields(const OrganizationEntity &entity) {
   return std::visit([](const auto &value) -> const OrganizationFields & { return value; }, entity);
}
OrganizationKey organizationKey(const OrganizationEntity &entity) {
   return {static_cast<OrganizationKind>(entity.index()), organizationFields(entity).id};
}
std::string organizationTitle(const OrganizationEntity &entity) {
   return std::visit([](const auto &value) {
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Pillar>) { return value.name; }
      else { return value.title; }
   }, entity);
}
void setOrganizationTitle(OrganizationEntity &entity, const std::string &title) {
   const auto normalized = normalizeTitle(title);
   std::visit([&](auto &value) {
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Pillar>) { value.name = normalized; }
      else { value.title = normalized; }
   }, entity);
}
std::optional<OrganizationKey> organizationParent(const OrganizationEntity &entity) {
   return std::visit([](const auto &value) -> std::optional<OrganizationKey> {
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Pillar>) {
         if (value.parentPillarId) { return OrganizationKey{OrganizationKind::PILLAR, *value.parentPillarId}; }
      } else {
         if (value.pillarId) { return OrganizationKey{OrganizationKind::PILLAR, *value.pillarId}; }
         if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Project>) {
            if (value.parentProjectId) { return OrganizationKey{OrganizationKind::PROJECT, *value.parentProjectId}; }
         } else {
            if (value.projectId) { return OrganizationKey{OrganizationKind::PROJECT, *value.projectId}; }
         }
      }
      return {};
   }, entity);
}
void setOrganizationParent(OrganizationEntity &entity, const std::optional<OrganizationKey> &parent) {
   if (parent && (!isUuid(parent->id) || parent->kind == OrganizationKind::OBJECTIVE)) {
      throw std::invalid_argument("A parent must be a pillar or project.");
   }
   std::visit([&](auto &value) {
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Pillar>) {
         if (parent && parent->kind != OrganizationKind::PILLAR) {
            throw std::invalid_argument("Pillars may only belong to pillars.");
         }
         value.parentPillarId = parent ? std::optional(parent->id) : std::nullopt;
      } else {
         if (!parent) { throw std::invalid_argument("Projects and objectives require a parent."); }
         value.pillarId = parent->kind == OrganizationKind::PILLAR ? std::optional(parent->id) : std::nullopt;
         auto project = parent->kind == OrganizationKind::PROJECT ? std::optional(parent->id) : std::nullopt;
         if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Project>) { value.parentProjectId = project; }
         else { value.projectId = project; }
      }
   }, entity);
}
void validateOrganization(const OrganizationEntity &entity) {
   const auto &fields = organizationFields(entity);
   // Shared title, UTF-8/description, UUID and timestamp/revision rules.
   Task check{fields.id, fields.datasetId, organizationTitle(entity), fields.description,
              TaskStatus::ACTIVE, fields.createdAt, fields.updatedAt, {}, fields.deletedAt, fields.localRevision};
   validateTask(check);
   if (fields.archivedAt && (*fields.archivedAt < fields.createdAt || *fields.archivedAt > fields.updatedAt)) {
      throw std::invalid_argument("Invalid archive timestamp.");
   }
   std::visit([](const auto &value) {
      if constexpr (!std::is_same_v<std::decay_t<decltype(value)>, Pillar>) {
         const auto project = [&]() {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Project>) { return value.parentProjectId; }
            else { return value.projectId; }
         }();
         if (value.pillarId.has_value() == project.has_value()) {
            throw std::invalid_argument("Exactly one pillar or project parent is required.");
         }
      }
   }, entity);
   const auto parent = organizationParent(entity);
   if (parent && (!isUuid(parent->id) || *parent == organizationKey(entity))) {
      throw std::invalid_argument("Invalid or self parent.");
   }
}

OrganizationRules::OrganizationRules(std::string datasetId, const std::vector<OrganizationEntity> &values)
   : datasetId(std::move(datasetId)) {
   if (!isUuid(this->datasetId)) { throw std::invalid_argument("Invalid organization dataset."); }
   for (const auto &entity : values) {
      validateOrganization(entity);
      if (organizationFields(entity).datasetId != this->datasetId
          || !entities.emplace(organizationKey(entity), entity).second) {
         throw std::invalid_argument("Duplicate or cross-dataset organization record.");
      }
   }
   // Validate all paths iteratively, with memoization: deep trees remain O(nodes).
   std::set<OrganizationKey> complete;
   for (const auto &[key, entity] : entities) {
      (void)entity;
      std::set<OrganizationKey> visiting;
      std::vector<OrganizationKey> chain;
      std::optional<OrganizationKey> current = key;
      while (current && !complete.contains(*current)) {
         if (!visiting.insert(*current).second) { throw std::invalid_argument("Organization cycle detected."); }
         chain.push_back(*current);
         current = organizationParent(find(*current));
      }
      complete.insert(chain.begin(), chain.end());
   }
}
const OrganizationEntity &OrganizationRules::find(const OrganizationKey &key) const {
   const auto found = entities.find(key);
   if (found == entities.end()) { throw std::invalid_argument("Organizational parent is missing or in another dataset."); }
   return found->second;
}
std::vector<OrganizationKey> OrganizationRules::path(const OrganizationKey &key) const {
   std::vector<OrganizationKey> result;
   std::optional<OrganizationKey> current = key;
   while (current) {
      result.push_back(*current);
      current = organizationParent(find(*current));
   }
   std::reverse(result.begin(), result.end());
   return result;
}
bool OrganizationRules::effectivelyArchived(const OrganizationKey &key) const {
   for (const auto &ancestor : path(key)) {
      if (organizationFields(find(ancestor)).archivedAt) { return true; }
   }
   return false;
}
bool OrganizationRules::taskArchived(const Task &task) const {
   requireTaskOwner(task, true);
   if (task.owner.kind == TaskOwnerKind::INDEPENDENT) { return false; }
   return effectivelyArchived({task.owner.kind == TaskOwnerKind::OBJECTIVE ? OrganizationKind::OBJECTIVE : OrganizationKind::PROJECT, task.owner.id});
}
void OrganizationRules::requireAvailableParent(const OrganizationEntity &entity, bool allowArchived) const {
   validateOrganization(entity);
   if (organizationFields(entity).datasetId != datasetId) { throw std::invalid_argument("Cross-dataset organization."); }
   if (const auto parent = organizationParent(entity)) {
      for (const auto &ancestor : path(*parent)) {
         if (ancestor == organizationKey(entity)) { throw std::invalid_argument("Move would create a cycle."); }
         if (organizationFields(find(ancestor)).deletedAt) { throw std::invalid_argument("Parent is removed."); }
      }
      if (!allowArchived && effectivelyArchived(*parent)) { throw std::invalid_argument("Parent is archived; explicit confirmation is required for a move."); }
   }
}
void OrganizationRules::requireTaskOwner(const Task &task, bool allowArchived) const {
   validateTask(task);
   if (task.datasetId != datasetId) { throw std::invalid_argument("Cross-dataset task."); }
   if (task.owner.kind == TaskOwnerKind::INDEPENDENT) { return; }
   const OrganizationKey parent{task.owner.kind == TaskOwnerKind::OBJECTIVE ? OrganizationKind::OBJECTIVE : OrganizationKind::PROJECT, task.owner.id};
   for (const auto &ancestor : path(parent)) {
      if (organizationFields(find(ancestor)).deletedAt) { throw std::invalid_argument("Task owner is removed."); }
   }
   if (!allowArchived && effectivelyArchived(parent)) { throw std::invalid_argument("Task owner is archived; confirm the move."); }
}
void OrganizationRules::requireRemovable(const OrganizationKey &key, const std::vector<Task> &tasks) const {
   find(key);
   for (const auto &[childKey, entity] : entities) {
      (void)childKey;
      if (!organizationFields(entity).deletedAt && organizationParent(entity) == key) {
         throw std::invalid_argument("Cannot remove organization with children, including archived children.");
      }
   }
   for (const auto &task : tasks) {
      if (task.deletedAt || task.owner.kind == TaskOwnerKind::INDEPENDENT) { continue; }
      const OrganizationKey owner{task.owner.kind == TaskOwnerKind::OBJECTIVE ? OrganizationKind::OBJECTIVE : OrganizationKind::PROJECT, task.owner.id};
      if (owner == key) { throw std::invalid_argument("Cannot remove organization with tasks, including completed tasks."); }
   }
}

}
