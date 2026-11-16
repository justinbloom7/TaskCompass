#include "domain/OrganizationRules.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace taskcompass;
namespace {
void check(bool value) { if (!value) { throw std::runtime_error("Organization rule assertion failed"); } }
template<typename Action> void rejects(Action action) {
   bool failed = false;
   try { action(); } catch (const std::exception &) { failed = true; }
   check(failed);
}
std::string uuid(int value) {
   std::ostringstream result;
   result << "00000000-0000-4000-8000-" << std::setfill('0') << std::setw(12) << value;
   return result.str();
}
void initialize(OrganizationFields &value, int id) {
   value.id = uuid(id); value.datasetId = uuid(1);
   value.createdAt = 1000; value.updatedAt = 2000; value.localRevision = 1;
}
}
int main() {
   try {
      Pillar root; initialize(root, 2); root.name = "Home";
      Project project; initialize(project, 3); project.title = "Repair"; project.pillarId = root.id;
      Objective objective; initialize(objective, 4); objective.title = "Ready"; objective.projectId = project.id;
      std::vector<OrganizationEntity> values{root, project, objective};
      OrganizationRules graph(uuid(1), values);
      check(graph.path(organizationKey(values[2])).size() == 3);
      Task task{uuid(5), uuid(1), "Task", {}, TaskStatus::COMPLETED, 1000, 2000, 2000, {}, 1,
                {TaskOwnerKind::OBJECTIVE, objective.id}};
      graph.requireTaskOwner(task);
      rejects([&]() { graph.requireRemovable({OrganizationKind::OBJECTIVE, objective.id}, {task}); });
      rejects([&]() { graph.requireRemovable({OrganizationKind::PILLAR, root.id}, {}); });
      auto bad = project; bad.parentProjectId = uuid(3);
      rejects([&]() { validateOrganization(bad); });
      bad = project; bad.pillarId.reset();
      rejects([&]() { validateOrganization(bad); });
      auto other = objective; other.datasetId = uuid(99);
      rejects([&]() { OrganizationRules invalid(uuid(1), {root, project, other}); });
      OrganizationEntity moved = root;
      rejects([&]() { setOrganizationParent(moved, OrganizationKey{OrganizationKind::OBJECTIVE, objective.id}); });
      setOrganizationParent(moved, OrganizationKey{OrganizationKind::PILLAR, uuid(6)});
      Pillar child; initialize(child, 6); child.name = "Child"; child.parentPillarId = root.id;
      OrganizationRules withChild(uuid(1), {root, child});
      rejects([&]() { withChild.requireAvailableParent(moved); });
      rejects([&]() { OrganizationRules cycle(uuid(1), {moved, child}); });
      root.archivedAt = 1500; objective.archivedAt = 1600;
      OrganizationRules archived(uuid(1), {root, project, objective});
      check(archived.taskArchived(task));
      rejects([&]() { archived.requireTaskOwner(task); });
      root.archivedAt.reset();
      check(OrganizationRules(uuid(1), {root, project, objective}).taskArchived(task));
      objective.archivedAt.reset();
      check(!OrganizationRules(uuid(1), {root, project, objective}).taskArchived(task));
      // Complete legal ownership alternatives, without redundant task context.
      Objective direct; initialize(direct, 7); direct.title = "Direct outcome"; direct.pillarId = root.id;
      Project subproject; initialize(subproject, 8); subproject.title = "Subproject"; subproject.parentProjectId = project.id;
      OrganizationRules ownership(uuid(1), {root, project, subproject, objective, direct});
      auto independent = task; independent.owner = {}; ownership.requireTaskOwner(independent);
      auto directTask = task; directTask.owner = {TaskOwnerKind::PROJECT, subproject.id}; ownership.requireTaskOwner(directTask);
      auto directOutcomeTask = task; directOutcomeTask.owner = {TaskOwnerKind::OBJECTIVE, direct.id}; ownership.requireTaskOwner(directOutcomeTask);
      auto missing = task; missing.owner = {TaskOwnerKind::PROJECT, uuid(99)};
      rejects([&]() { ownership.requireTaskOwner(missing); });
      OrganizationEntity wrongParent = subproject;
      rejects([&]() { setOrganizationParent(wrongParent, {}); });
      auto badObjective = direct; badObjective.projectId = project.id;
      rejects([&]() { validateOrganization(badObjective); });
      badObjective.pillarId.reset(); badObjective.projectId.reset();
      rejects([&]() { validateOrganization(badObjective); });
      // Deep self-FK paths do not depend on recursion or database CTE depth.
      values = {root};
      std::string previous = root.id;
      for (int index = 10; index < 2010; ++index) {
         Pillar pillar; initialize(pillar, index); pillar.name = "Nested"; pillar.parentPillarId = previous;
         previous = pillar.id; values.emplace_back(pillar);
      }
      OrganizationRules deep(uuid(1), values);
      check(deep.path({OrganizationKind::PILLAR, previous}).size() == 2001);
      std::string projectParent = project.id;
      values = {root, project};
      for (int index = 3000; index < 5000; ++index) {
         Project nested; initialize(nested, index); nested.title = "Nested project"; nested.parentProjectId = projectParent;
         projectParent = nested.id; values.emplace_back(nested);
      }
      check(OrganizationRules(uuid(1), values).path({OrganizationKind::PROJECT, projectParent}).size() == 2002);
      std::cout << "PASS: typed parent choices, dataset guards, cycles, inherited archive, dependency guards and 2001-level iterative hierarchy.\n";
      return 0;
   } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
