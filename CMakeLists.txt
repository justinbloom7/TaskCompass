cmake_minimum_required(VERSION 3.21)
project(TaskCompassPrototype VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(MSVC)
   add_compile_options(/utf-8 /W4)
else()
   add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Sql)

add_library(PrototypeDesktop STATIC
            src/desktop/MainWindow.cpp
            src/desktop/MainWindow.h
            src/desktop/models/SampleTask.h
            src/desktop/models/TaskTableModel.cpp
            src/desktop/models/TaskTableModel.h
            src/desktop/models/TaskFilterModel.cpp
            src/desktop/models/TaskFilterModel.h
            src/desktop/dialogs/TaskEditDialog.cpp
            src/desktop/dialogs/TaskEditDialog.h)
target_include_directories(PrototypeDesktop PUBLIC src)
target_link_libraries(PrototypeDesktop PUBLIC Qt6::Widgets)

add_executable(TaskCompassPrototype src/desktop/Main.cpp)
target_link_libraries(TaskCompassPrototype PRIVATE PrototypeDesktop)

include(CTest)
if(BUILD_TESTING)
   find_package(Python3 QUIET COMPONENTS Interpreter)
   if(Python3_Interpreter_FOUND)
      add_test(NAME DatabaseSchemaTests COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/DatabaseSchemaTests.py")
   endif()
endif()

add_executable(TaskCompassDbDiagnostic src/infrastructure/storage/MariaDbDiagnostic.cpp)
target_link_libraries(TaskCompassDbDiagnostic PRIVATE Qt6::Sql)

if(BUILD_TESTING)
   find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
   add_executable(PrototypeTests tests/PrototypeTests.cpp)
   target_link_libraries(PrototypeTests PRIVATE PrototypeDesktop Qt6::Test)
   add_dependencies(PrototypeTests TaskCompassDbDiagnostic)
   add_test(NAME PrototypeTests
            COMMAND PrototypeTests $<TARGET_FILE:TaskCompassDbDiagnostic>)
   add_test(NAME GuiStartup COMMAND TaskCompassPrototype --smoke-test)
   find_program(TASKCOMPASS_BASH_EXECUTABLE NAMES bash)
   if(TASKCOMPASS_BASH_EXECUTABLE)
      # MSYS2 mock suites launch many short-lived shell processes; Windows startup
      # overhead is substantially higher than Linux. Keep finite, platform-sized limits.
      if(WIN32)
         set(TASKCOMPASS_SHELL_TEST_TIMEOUT 180)
         set(TASKCOMPASS_PHASE11_SCRIPT_TIMEOUT 180)
      else()
         set(TASKCOMPASS_SHELL_TEST_TIMEOUT 30)
         set(TASKCOMPASS_PHASE11_SCRIPT_TIMEOUT 60)
      endif()
      add_test(NAME DevDatabaseScripts
               COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/DevDatabaseScripts.test.sh")
      set_tests_properties(DevDatabaseScripts PROPERTIES TIMEOUT ${TASKCOMPASS_SHELL_TEST_TIMEOUT})
      add_test(NAME IntegrationDatabaseScripts
               COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/IntegrationDatabaseScripts.test.sh")
      set_tests_properties(IntegrationDatabaseScripts PROPERTIES TIMEOUT ${TASKCOMPASS_SHELL_TEST_TIMEOUT})
      add_test(NAME Phase12ValidationScripts COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/Phase12ValidationScripts.test.sh")
      set_tests_properties(Phase12ValidationScripts PROPERTIES TIMEOUT ${TASKCOMPASS_PHASE11_SCRIPT_TIMEOUT})
      add_test(NAME Phase11ValidationScripts
               COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/Phase11ValidationScripts.test.sh")
      set_tests_properties(Phase11ValidationScripts PROPERTIES TIMEOUT ${TASKCOMPASS_PHASE11_SCRIPT_TIMEOUT})
   else()
      message(STATUS "Bash unavailable: skipping database helper mock tests")
   endif()
   set_tests_properties(PrototypeTests GuiStartup PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                        TIMEOUT 30)
endif()

# Phase 1.1: standard-C++ core, Qt SQL adapter and separate persistent desktop.
add_library(TaskCore STATIC src/domain/Task.cpp src/domain/OrganizationRules.cpp
            src/application/CommandService.cpp src/application/TaskService.cpp)
set_target_properties(TaskCore PROPERTIES AUTOMOC OFF)
target_include_directories(TaskCore PUBLIC src)

set(CMAKE_AUTORCC ON)
add_library(TaskStorage STATIC
            src/infrastructure/storage/DatabaseConnection.cpp
            src/infrastructure/storage/DatabaseConfig.cpp
            src/infrastructure/storage/MigrationRunner.cpp
            src/infrastructure/storage/SchemaConstraints.cpp
            src/infrastructure/storage/MutationLease.cpp
            src/infrastructure/storage/MariaDbTaskRepository.cpp
            src/infrastructure/storage/MariaDbWorkspaceRepository.cpp
            src/infrastructure/storage/WorkspaceCodec.cpp
            src/infrastructure/storage/Migrations.qrc)
target_link_libraries(TaskStorage PUBLIC TaskCore Qt6::Sql)
add_executable(TaskCompassMigrate src/infrastructure/storage/MigrateMain.cpp)
target_link_libraries(TaskCompassMigrate PRIVATE TaskStorage)

if(BUILD_TESTING)
   add_executable(TaskCoreTests tests/TaskCoreTests.cpp)
   set_target_properties(TaskCoreTests PROPERTIES AUTOMOC OFF)
   target_link_libraries(TaskCoreTests PRIVATE TaskCore)
   add_test(NAME TaskCoreTests COMMAND TaskCoreTests)
   add_executable(OrganizationRulesTests tests/OrganizationRulesTests.cpp)
   set_target_properties(OrganizationRulesTests PROPERTIES AUTOMOC OFF)
   target_link_libraries(OrganizationRulesTests PRIVATE TaskCore)
   add_test(NAME OrganizationRulesTests COMMAND OrganizationRulesTests)
   add_executable(CommandServiceTests tests/CommandServiceTests.cpp)
   set_target_properties(CommandServiceTests PROPERTIES AUTOMOC OFF)
   target_link_libraries(CommandServiceTests PRIVATE TaskCore)
   add_test(NAME CommandServiceTests COMMAND CommandServiceTests)
   add_test(NAME MigrationSqlPreview COMMAND TaskCompassMigrate)
endif()

add_library(PersistentDesktop STATIC
            src/desktop/dialogs/OrganizationEditDialog.cpp
            src/desktop/dialogs/OrganizationMoveDialog.cpp
            src/desktop/models/OrganizationTreeModel.cpp
            src/desktop/persistent/PersistentTaskController.cpp
            src/desktop/persistent/PersistentTaskModel.cpp
            src/desktop/persistent/PersistentTaskFilter.cpp
            src/desktop/persistent/PersistentTaskWindow.cpp)
target_link_libraries(PersistentDesktop PUBLIC TaskCore Qt6::Widgets)
add_executable(TaskCompass src/desktop/persistent/PersistentMain.cpp)
target_link_libraries(TaskCompass PRIVATE PersistentDesktop TaskStorage)

if(BUILD_TESTING)
   add_executable(PersistentDesktopTests tests/PersistentDesktopTests.cpp)
   target_link_libraries(PersistentDesktopTests PRIVATE PersistentDesktop Qt6::Test)
   add_test(NAME PersistentDesktopTests COMMAND PersistentDesktopTests)
   set_tests_properties(PersistentDesktopTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
   add_executable(OrganizationDesktopTests tests/OrganizationDesktopTests.cpp)
   target_link_libraries(OrganizationDesktopTests PRIVATE PersistentDesktop Qt6::Test)
   add_test(NAME OrganizationDesktopTests COMMAND OrganizationDesktopTests)
   set_tests_properties(OrganizationDesktopTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
endif()

if(BUILD_TESTING)
   add_executable(MariaDbIntegrationTests tests/MariaDbIntegrationTests.cpp)
   target_link_libraries(MariaDbIntegrationTests PRIVATE TaskStorage)
   option(TASKCOMPASS_ENABLE_DB_TESTS "Enable explicitly approved destructive tests on taskcompass_integration only" OFF)
   if(TASKCOMPASS_ENABLE_DB_TESTS)
      add_test(NAME MariaDbIntegrationTests COMMAND MariaDbIntegrationTests --run --allow-reset)
   else()
      add_test(NAME MariaDbIntegrationTests COMMAND MariaDbIntegrationTests)
   endif()
   set_tests_properties(MariaDbIntegrationTests PROPERTIES SKIP_RETURN_CODE 77 LABELS integration TIMEOUT 120)
   find_package(Threads REQUIRED)
   add_executable(OrganizationIntegrationTests tests/OrganizationIntegrationTests.cpp)
   target_link_libraries(OrganizationIntegrationTests PRIVATE TaskStorage Threads::Threads)
   option(TASKCOMPASS_ENABLE_PHASE12_DB_TESTS "Enable separately approved Phase 1.2 tests on fresh taskcompass_phase12_test only" OFF)
   if(TASKCOMPASS_ENABLE_PHASE12_DB_TESTS)
      add_test(NAME OrganizationIntegrationTests COMMAND OrganizationIntegrationTests --run --allow-reset)
   else()
      add_test(NAME OrganizationIntegrationTests COMMAND OrganizationIntegrationTests)
   endif()
   set_tests_properties(OrganizationIntegrationTests PROPERTIES SKIP_RETURN_CODE 77 LABELS integration TIMEOUT 600)
endif()

if(BUILD_TESTING)
   add_executable(SchemaConstraintsTests tests/SchemaConstraintsTests.cpp)
   target_link_libraries(SchemaConstraintsTests PRIVATE TaskStorage)
   add_test(NAME SchemaConstraintsTests COMMAND SchemaConstraintsTests)
   add_executable(StorageSafetyTests tests/StorageSafetyTests.cpp)
   add_executable(WorkspaceCodecTests tests/WorkspaceCodecTests.cpp)
   target_link_libraries(WorkspaceCodecTests PRIVATE TaskStorage)
   add_dependencies(WorkspaceCodecTests TaskCompassMigrate OrganizationIntegrationTests)
   add_test(NAME WorkspaceCodecTests COMMAND WorkspaceCodecTests
            "${CMAKE_CURRENT_SOURCE_DIR}/migrations/mariadb/002_OrganizationalManagement.sql"
            $<TARGET_FILE:TaskCompassMigrate> $<TARGET_FILE:OrganizationIntegrationTests>)
   target_link_libraries(StorageSafetyTests PRIVATE TaskStorage)
   add_dependencies(StorageSafetyTests MariaDbIntegrationTests)
   add_test(NAME StorageSafetyTests COMMAND StorageSafetyTests
            "${CMAKE_CURRENT_SOURCE_DIR}/migrations/mariadb/001_PersistentTaskFoundation.sql"
            $<TARGET_FILE:MariaDbIntegrationTests>)
   set_tests_properties(MigrationSqlPreview PROPERTIES PASS_REGULAR_EXPRESSION "CREATE TABLE Task")
endif()
