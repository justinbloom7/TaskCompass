#include "MemoryWorkspaceRepository.h"
#include "application/CommandService.h"
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace taskcompass;
namespace {
void check(bool condition, const char *message) { if (!condition) { throw std::runtime_error(message); } }
template<typename Action> void rejects(Action action) {
   bool rejected = false;
   try { action(); } catch (const std::exception &) { rejected = true; }
   check(rejected, "Expected rejection");
}
std::string uuid() {
   static int counter = 0;
   std::ostringstream value;
   value << "00000000-0000-4000-8000-" << std::setfill('0') << std::setw(12) << ++counter;
   return value.str();
}
OrganizationCommand create(OrganizationKind kind, const std::string &title, std::optional<OrganizationKey> parent = {}) {
   OrganizationCommand command;
   command.kind = kind; command.title = title; command.parent = parent;
   return command;
}
OrganizationCommand change(const WorkspaceReceipt &receipt, OrganizationOperation operation) {
   OrganizationCommand command;
   command.kind = organizationKey(asOrganization(receipt.after)).kind;
   command.id = entityKey(receipt.after).id; command.expectedRevision = entityRevision(receipt.after);
   command.operation = operation;
   return command;
}
}
int main() {
   try {
      MemoryWorkspaceRepository repository;
      const auto dataset = uuid();
      Timestamp now = 1000;
      CommandService service(repository, dataset, [&]() { return now++; }, uuid);
      auto pillar = service.execute(create(OrganizationKind::PILLAR, "Responsibility"));
      OrganizationKey pillarKey{OrganizationKind::PILLAR, entityKey(pillar.after).id};
      auto project = service.execute(create(OrganizationKind::PROJECT, "Work", pillarKey));
      OrganizationKey projectKey{OrganizationKind::PROJECT, entityKey(project.after).id};
      auto objective = service.execute(create(OrganizationKind::OBJECTIVE, "Outcome", projectKey));
      TaskCommand capture{TaskOperation::CREATE, {}, 0, "Task", {}};
      capture.ownerChange = {TaskOwnerKind::OBJECTIVE, entityKey(objective.after).id};
      auto task = service.execute(capture);
      check(service.undoDescription() == "Create task", "Wrong global history head");
      auto completed = service.execute(TaskCommand{TaskOperation::COMPLETE, entityKey(task.after).id, 1, {}, {}});
      const auto completedAt = std::get<Task>(completed.after).completedAt;
      service.undo(); service.redo();
      check(repository.snapshot.tasks[0].completedAt == completedAt, "Completion timestamp inverse drift");
      auto archive = change(pillar, OrganizationOperation::ARCHIVE);
      auto archived = service.execute(archive);
      check(OrganizationRules(dataset, service.read().organizations).taskArchived(repository.snapshot.tasks[0]), "Archive not inherited");
      check(!organizationFields(repository.snapshot.organizations[1]).archivedAt, "Archive rewrote child flag");
      auto stale = change(project, OrganizationOperation::MOVE);
      stale.parent = pillarKey; stale.expectedWorkspace = workspaceRevisions(repository.snapshot);
      service.execute(create(OrganizationKind::PILLAR, "Other"));
      rejects([&]() { service.execute(stale); });
      service.undo();
      check(service.canRedo(), "No redo after undo");
      repository.failAfter = 1;
      const auto counts = repository.receipts.size();
      rejects([&]() { service.undo(); });
      check(repository.receipts.size() == counts && service.canRedo(), "Failure changed history");
      repository.failAfter = -1;
      service.undo(); // Undo ancestor archive.
      check(!OrganizationRules(dataset, service.read().organizations).taskArchived(repository.snapshot.tasks[0]), "Undo archive failed");
      service.redo();
      check(OrganizationRules(dataset, service.read().organizations).taskArchived(repository.snapshot.tasks[0]), "Redo archive failed");
      auto restore = change(archived, OrganizationOperation::RESTORE);
      restore.expectedRevision = organizationFields(repository.snapshot.organizations[0]).localRevision;
      service.execute(restore);
      check(!OrganizationRules(dataset, service.read().organizations).taskArchived(repository.snapshot.tasks[0]), "Restore failed");
      // Concurrent unrelated edit on the same task must not be overwritten by undo.
      auto current = repository.snapshot.tasks[0];
      auto edit = service.execute(TaskCommand{TaskOperation::EDIT, current.id, current.localRevision, "Changed", "Notes"});
      repository.snapshot.tasks[0].notes = "External notes";
      ++repository.snapshot.tasks[0].localRevision;
      rejects([&]() { service.undo(); });
      check(repository.snapshot.tasks[0].notes == "External notes", "Stale undo overwrote external fields");
      current = repository.snapshot.tasks[0];
      service.execute(TaskCommand{TaskOperation::EDIT, current.id, current.localRevision, "Reviewed", current.notes});
      service.undo();
      check(!service.canUndo(), "Accepted external revision revived old history");
      (void)edit;
      CommandService restarted(repository, dataset, [&]() { return now++; }, uuid);
      check(!restarted.canUndo() && restarted.read().tasks.size() == 1, "Restart history/state policy");

      MemoryWorkspaceRepository independent;
      CommandService createUndo(independent, dataset, [&]() { return now++; }, uuid);
      auto parent = createUndo.execute(create(OrganizationKind::PILLAR, "Parent"));
      auto child = createUndo.execute(create(OrganizationKind::PROJECT, "Child", OrganizationKey{OrganizationKind::PILLAR, entityKey(parent.after).id}));
      createUndo.undo(); createUndo.undo(); createUndo.redo(); createUndo.redo();
      check(!organizationFields(independent.snapshot.organizations[1]).deletedAt, "Child/parent inverse sequence failed");
      // An external dependent added after a create must prevent undo-create.
      createUndo.clearHistory();
      auto leaf = createUndo.execute(create(OrganizationKind::OBJECTIVE, "Leaf", OrganizationKey{OrganizationKind::PROJECT, entityKey(child.after).id}));
      Task dependent{uuid(), dataset, "Completed dependent", {}, TaskStatus::COMPLETED, 1000, 2000, 2000, {}, 1,
                     {TaskOwnerKind::OBJECTIVE, entityKey(leaf.after).id}};
      independent.snapshot.tasks.push_back(dependent);
      rejects([&]() { createUndo.undo(); });
      check(createUndo.canUndo(), "Rejected dependency inverse advanced stack");
      independent.snapshot.tasks.clear();
      createUndo.undo();
      independent.uncertain = true;
      rejects([&]() { createUndo.redo(); });
      check(!createUndo.canUndo() && !createUndo.canRedo(), "Unknown commit did not clear history");
      MemoryWorkspaceRepository movedRepository;
      CommandService moves(movedRepository, dataset, [&]() { return now++; }, uuid);
      const auto rootA = moves.execute(create(OrganizationKind::PILLAR, "A"));
      const auto rootB = moves.execute(create(OrganizationKind::PILLAR, "B"));
      const OrganizationKey rootAKey{OrganizationKind::PILLAR, entityKey(rootA.after).id};
      const OrganizationKey rootBKey{OrganizationKind::PILLAR, entityKey(rootB.after).id};
      const auto branch = moves.execute(create(OrganizationKind::PROJECT, "Branch", rootAKey));
      const OrganizationKey branchKey{OrganizationKind::PROJECT, entityKey(branch.after).id};
      const auto nested = moves.execute(create(OrganizationKind::PROJECT, "Nested", branchKey));
      const OrganizationKey nestedKey{OrganizationKind::PROJECT, entityKey(nested.after).id};
      const auto outcome = moves.execute(create(OrganizationKind::OBJECTIVE, "Flat", nestedKey));
      const OrganizationKey outcomeKey{OrganizationKind::OBJECTIVE, entityKey(outcome.after).id};
      TaskCommand ownedCapture{TaskOperation::CREATE, {}, 0, "Owned", {}};
      ownedCapture.ownerChange = {TaskOwnerKind::OBJECTIVE, outcomeKey.id};
      const auto ownedTask = moves.execute(ownedCapture);
      const auto beforeSubtree = movedRepository.snapshot;
      auto moveBranch = change(branch, OrganizationOperation::MOVE); moveBranch.parent = rootBKey;
      moveBranch.expectedWorkspace = workspaceRevisions(beforeSubtree);
      moves.execute(moveBranch);
      check(OrganizationRules(dataset, moves.read().organizations).path(outcomeKey).front() == rootBKey, "Subtree context failed");
      check(movedRepository.snapshot.tasks == beforeSubtree.tasks, "Subtree move rewrote task identity/state");
      check(movedRepository.snapshot.organizations[3] == beforeSubtree.organizations[3]
            && movedRepository.snapshot.organizations[4] == beforeSubtree.organizations[4], "Move rewrote descendant fields/revisions");
      moves.undo();
      check(OrganizationRules(dataset, moves.read().organizations).path(outcomeKey).front() == rootAKey, "Move inverse failed");
      movedRepository.failAfter = 0;
      rejects([&]() { moves.redo(); });
      check(moves.canRedo(), "Failed redo discarded history");
      movedRepository.failAfter = -1; moves.redo();
      const auto explicitArchive = moves.execute(change(outcome, OrganizationOperation::ARCHIVE));
      const auto ancestorArchive = moves.execute(change(rootB, OrganizationOperation::ARCHIVE));
      moves.execute(change(ancestorArchive, OrganizationOperation::RESTORE));
      check(OrganizationRules(dataset, moves.read().organizations).taskArchived(std::get<Task>(ownedTask.after)), "Ancestor restore cleared descendant flag");
      moves.execute(change(explicitArchive, OrganizationOperation::RESTORE));
      check(!OrganizationRules(dataset, moves.read().organizations).taskArchived(std::get<Task>(ownedTask.after)), "Explicit restore failed");
      std::cout << "PASS: shared typed history, lifecycle/ownership, archive inheritance, mixed inverses, rollback/redo, external edits and unknown commits.\n";
      return 0;
   } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
