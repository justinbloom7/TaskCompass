#pragma once

#include "application/TaskService.h"
#include "application/CommandService.h"
#include <memory>

// Composition owned entirely by the database worker. The opaque connection owner
// keeps Qt SQL types out of the application API and lets desktop tests use a fake.
struct PersistentTaskSession {
   std::shared_ptr<void> connectionLifetime;
   std::unique_ptr<taskcompass::TaskRepository> repository;
   std::unique_ptr<taskcompass::TaskService> service;
   std::unique_ptr<taskcompass::WorkspaceRepository> workspaceRepository;
   std::unique_ptr<taskcompass::CommandService> commands;
};

