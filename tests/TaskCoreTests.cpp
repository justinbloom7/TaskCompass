#include "MemoryTaskRepository.h"
#include "application/TaskService.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace taskcompass;

namespace {
void check(bool condition, const char *message) {
   if (!condition) {
      throw std::runtime_error(message);
   }
}
template<typename Operation> void rejects(Operation operation) {
   bool rejected = false;
   try { operation(); } catch (const std::exception &) { rejected = true; }
   check(rejected, "Expected operation to fail");
}
std::string nextId() {
   static unsigned counter = 0;
   std::ostringstream value;
   value << "00000000-0000-4000-8000-" << std::setfill('0') << std::setw(12) << ++counter;
   return value.str();
}
}

int main() {
   try {
      MemoryTaskRepository repository;
      Timestamp now = 1000;
      const auto datasetId = nextId();
      TaskService service(repository, datasetId, [&now]() { return now; }, nextId);
      rejects([&]() { service.execute({TaskOperation::CREATE, {}, 0, " \t\n", {}}); });
      rejects([&]() { service.execute({TaskOperation::CREATE, {}, 0, std::string(1025, 'x'), {}}); });
      rejects([&]() { service.execute({TaskOperation::CREATE, {}, 0, std::string("\xc0\x80"), {}}); });
      rejects([&]() { service.execute({TaskOperation::CREATE, {}, 0, "Valid", std::string("\xed\xa0\x80")}); });
      check(repository.tasks.empty() && !service.canUndo(), "Invalid input mutated state");
      auto created = service.execute({TaskOperation::CREATE, {}, 0, "  Task 🧭  ", "Notes"});
      const auto id = created.after.id;
      check(created.after.title == "Task 🧭" && created.after.createdAt == 1000
            && created.after.localRevision == 1 && !created.after.completedAt, "Bad capture");
      auto invalid = created.after;
      invalid.status = TaskStatus::COMPLETED;
      rejects([&]() { validateTask(invalid); });
      invalid.completedAt = 999;
      rejects([&]() { validateTask(invalid); });
      now = 2000;
      auto edited = service.execute({TaskOperation::EDIT, id, 1, "Edited", {}});
      check(edited.after.createdAt == 1000 && edited.after.updatedAt == 2000
            && !edited.after.notes, "Edit changed immutable data or null semantics");
      rejects([&]() { service.execute({TaskOperation::EDIT, id, 1, "Stale", {}}); });
      auto completed = service.execute({TaskOperation::COMPLETE, id, 2, {}, {}});
      const auto completionTime = completed.after.completedAt;
      check(completionTime && completed.after.status == TaskStatus::COMPLETED, "Completion failed");
      rejects([&]() { service.execute({TaskOperation::COMPLETE, id, 3, {}, {}}); });
      auto reopened = service.execute({TaskOperation::REOPEN, id, 3, {}, {}});
      check(!reopened.after.completedAt && reopened.after.status == TaskStatus::ACTIVE, "Reopen failed");
      check(service.undo().after.completedAt == completionTime, "Undo reopen lost completion timestamp");
      check(!service.undo().after.completedAt, "Undo completion failed");
      check(service.undo().after.title == created.after.title, "Undo edit failed across same-task revisions");
      check(service.undo().after.deletedAt.has_value() && service.list().empty(), "Undo create must tombstone");
      check(!service.canUndo() && service.canRedo(), "History index drift");
      check(service.redo().after.id == id && service.list().size() == 1, "Redo create changed identity");
      check(service.redo().after.title == "Edited", "Redo edit failed");
      check(service.redo().after.completedAt == completionTime, "Redo completion changed original timestamp");
      check(!service.redo().after.completedAt, "Redo reopen failed");
      check(!service.canRedo(), "Redo index drift");
      service.undo();
      const auto beforeFailure = repository.tasks;
      const auto historyCount = repository.receipts.size();
      repository.failCommit = true;
      rejects([&]() { service.redo(); });
      check(service.canRedo() && repository.tasks == beforeFailure && repository.receipts.size() == historyCount,
            "Failed redo changed history/state");
      rejects([&]() { service.undo(); });
      check(service.canUndo(), "Failed undo advanced history");
      rejects([&]() { service.execute({TaskOperation::EDIT, id, repository.tasks.at(id).localRevision, "Failed", {}}); });
      check(service.canRedo(), "Failed new command discarded redo");
      repository.failCommit = false;
      service.execute({TaskOperation::EDIT, id, repository.tasks.at(id).localRevision, "Branch", ""});
      check(!service.canRedo() && repository.tasks.at(id).notes == std::optional<std::string>(""), "Redo branch/null-empty distinction");
      ++repository.tasks.at(id).localRevision; // External writer: stale inverse must not overwrite it.
      rejects([&]() { service.undo(); });
      check(service.canUndo(), "Stale inverse advanced index");
      service.execute({TaskOperation::EDIT, id, repository.tasks.at(id).localRevision, "After external change", {}});
      service.undo();
      check(!service.canUndo(), "New edit revived stale inverses after an external revision");
      TaskService restarted(repository, datasetId, [&now]() { return now; }, nextId);
      check(restarted.list().size() == 1 && !restarted.canUndo(), "Restart history policy");
      for (int index = 0; index < 105; ++index) {
         restarted.execute({TaskOperation::CREATE, {}, 0, "Task", {}});
      }
      for (int index = 0; index < 100; ++index) {
         restarted.undo();
      }
      check(!restarted.canUndo(), "History limit failed");
      std::cout << "PASS: validation, lifecycle/timestamps, all inverse/redo commands, rollback contract, stale revisions, redo branches and history limit.\n";
      return 0;
   } catch (const std::exception &error) {
      std::cerr << error.what() << '\n';
      return 1;
   }
}
