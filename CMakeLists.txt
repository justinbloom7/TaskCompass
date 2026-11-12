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
   else()
      message(STATUS "Bash unavailable: skipping database helper mock tests")
   endif()
   set_tests_properties(PrototypeTests GuiStartup PROPERTIES
                        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                        TIMEOUT 30)
endif()
