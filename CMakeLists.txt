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
      add_test(NAME DevDatabaseScripts
               COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/DevDatabaseScripts.test.sh")
      set_tests_properties(DevDatabaseScripts PROPERTIES TIMEOUT 30)
      add_test(NAME IntegrationDatabaseScripts
               COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/IntegrationDatabaseScripts.test.sh")
      set_tests_properties(IntegrationDatabaseScripts PROPERTIES TIMEOUT 30)
      add_test(NAME Phase11ValidationScripts
               COMMAND "${TASKCOMPASS_BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/Phase11ValidationScripts.test.sh")
      set_tests_properties(Phase11ValidationScripts PROPERTIES TIMEOUT 60)
   else()
      message(STATUS "Bash unavailable: skipping database helper mock tests")
   endif()
   set_tests_properties(PrototypeTests GuiStartup PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                        TIMEOUT 30)
endif()

# Phase 1.1: standard-C++ core, Qt SQL adapter and separate persistent desktop.
add_library(TaskCore STATIC src/domain/Task.cpp src/application/TaskService.cpp)
set_target_properties(TaskCore PROPERTIES AUTOMOC OFF)
target_include_directories(TaskCore PUBLIC src)

set(CMAKE_AUTORCC ON)
add_library(TaskStorage STATIC
            src/infrastructure/storage/DatabaseConnection.cpp
            src/infrastructure/storage/DatabaseConfig.cpp
            src/infrastructure/storage/MigrationRunner.cpp
            src/infrastructure/storage/MariaDbTaskRepository.cpp
            src/infrastructure/storage/Migrations.qrc)
target_link_libraries(TaskStorage PUBLIC TaskCore Qt6::Sql)
add_executable(TaskCompassMigrate src/infrastructure/storage/MigrateMain.cpp)
target_link_libraries(TaskCompassMigrate PRIVATE TaskStorage)

if(BUILD_TESTING)
   add_executable(TaskCoreTests tests/TaskCoreTests.cpp)
   set_target_properties(TaskCoreTests PROPERTIES AUTOMOC OFF)
   target_link_libraries(TaskCoreTests PRIVATE TaskCore)
   add_test(NAME TaskCoreTests COMMAND TaskCoreTests)
   add_test(NAME MigrationSqlPreview COMMAND TaskCompassMigrate)
endif()

add_library(PersistentDesktop STATIC
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
endif()

if(BUILD_TESTING)
   add_executable(StorageSafetyTests tests/StorageSafetyTests.cpp)
   target_link_libraries(StorageSafetyTests PRIVATE TaskStorage)
   add_dependencies(StorageSafetyTests MariaDbIntegrationTests)
   add_test(NAME StorageSafetyTests COMMAND StorageSafetyTests
            "${CMAKE_CURRENT_SOURCE_DIR}/migrations/mariadb/001_PersistentTaskFoundation.sql"
            $<TARGET_FILE:MariaDbIntegrationTests>)
   set_tests_properties(MigrationSqlPreview PROPERTIES PASS_REGULAR_EXPRESSION "CREATE TABLE Task")
endif()
