#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Approve only the reviewed native Windows Stage C offline CTest inventory."""

from pathlib import Path, PurePosixPath
import json
import sys

OFFLINE_SUITES = {
   "DatabaseSchemaTests", "PrototypeTests", "GuiStartup", "DevDatabaseScripts",
   "IntegrationDatabaseScripts", "Phase12ValidationScripts", "Phase12BackupScripts",
   "Phase12RestoreScripts", "Phase12BackupAclPolicyTests", "Phase12RestoreAclTests",
   "Phase12Schema2RestoreTests", "Phase12Schema2BackupTests", "Phase12Schema2BackupScripts",
   "EnvironmentReportTests", "GuiAcceptanceEvidenceTests", "Phase12GuiAcceptanceScripts",
   "Schema2DdlComparison", "Phase12ReviewedCompletionScripts", "Phase12UpgradeInspectionScripts",
   "Phase12DevelopmentUpgradeScripts", "Phase12RuntimeRehearsalScripts",
   "Phase12MigrationRehearsalScripts", "Phase11ValidationScripts", "TaskCoreTests",
   "OrganizationRulesTests", "CommandServiceTests", "MigrationSqlPreview",
   "PersistentDesktopTests", "OrganizationDesktopTests", "SchemaConstraintsTests",
   "WorkspaceCodecTests", "StorageSafetyTests", "ContextStageTests",
}
LIVE_EXECUTABLES = {"mariadbintegrationtests", "organizationintegrationtests"}


def main():
   path = Path(sys.argv[1])
   tests = json.loads(path.read_text(encoding="utf-8"))["tests"]
   names = [test["name"] for test in tests]
   for test in tests:
      name = test["name"]
      command = test.get("command")
      if not command:
         raise SystemExit(f"CTest command missing for {name}. Rerun stage 03.")
      executable = PurePosixPath(command[0].replace("\\", "/")).stem.casefold()
      labels = {label.casefold() for prop in test.get("properties", [])
                if prop["name"] == "LABELS" for label in prop["value"]}
      if (executable in LIVE_EXECUTABLES or name.casefold() in LIVE_EXECUTABLES
            or labels & {"integration", "database", "database-backed", "live"}):
         raise SystemExit(f"Database-backed CTest entry blocked: {name}")
   unexpected = set(names) - OFFLINE_SUITES
   missing = OFFLINE_SUITES - set(names)
   if unexpected or missing or len(names) != len(set(names)):
      raise SystemExit(f"Unreviewed CTest inventory. Unexpected: {sorted(unexpected)}. "
                       f"Missing: {sorted(missing)}. Duplicate entries: {len(names) != len(set(names))}.")
   print(f"{len(names)} reviewed offline CTest entries. Both live integration suites are absent.")


if __name__ == "__main__":
   main()
