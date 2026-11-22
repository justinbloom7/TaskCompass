<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.10 — Phase 1.2 final reconciliation review

Current status: **2026-11-22 — owner dispositions recorded; final Phase 1.2
acceptance pending.** E1 is an owner-accepted three-field variance; its original
unchanged-record checkpoint remains FAILED/unsealed. E2 is owner-accepted
functional persistence; its historical byte comparison remains NOT CAPTURED.
See sections6–7 for the decisions and final acceptance recommendation.
The initial review proposals in sections2–5 are preserved as historical analysis,
superseded by the subsequent explicit owner decisions. No overall acceptance or
new testing is authorized by this document.

The [milestone](milestones/Phase1.2.md) retains the chronological approvals,
transcripts, failures, corrections and source/test results. This review reads that
record and relevant existing acceptance/test definitions. It does not independently
inspect private evidence or execute SQL, GUI, tests, backups or restoration.

## 1. Recorded acceptance evidence

Live results below are **owner-reported operator outcomes**. Agent-executed checks
are offline source/build/test/documentation results specifically identified in the
milestone. A script PASS is evidence of its fail-closed implementation's required
checks, not a claim that the agent observed private files or the live application.

| Gate | Linux | Windows | Reconciliation |
| --- | --- | --- | --- |
| Pre-B7 isolated integration | Corrected Stage1B passed, 2026-11-16, after separately approved fixture recovery | Stage1B passed, 2026-11-16 | Schema1→2 synthetic preservation, command/archive/history/atomic rollback, lease timeout/loss/conflicting moves and repository reconnection tested on both actual MariaDB versions. Repository reconnect is not a manual GUI restart. |
| B7.1 baseline | Schema1/UUID/fingerprint/grants passed, 2026-11-17 | Same-platform independent baseline passed | Known five-table state and least-privilege identities; earlier CHECK-catalog visibility/source corrections remain historical, followed by successful verification. |
| B7.2 private schema1 backup | Capture completed, private review and checksums passed, 2026-11-17 | Capture/private review/seals passed, 2026-11-17 | Independent exact platform snapshots preserved; failed prompts/path selections remain recorded. |
| B7.3 populated schema1 restore | Full source/data/history/DDL comparison passed, 2026-11-17 | Same passed, 2026-11-17 | Populated rehearsal target retained; no reset suite run on that retained clone. |
| B7.4a narrow-role migration rehearsal | Four stages passed, 2026-11-17 | Four stages passed, 2026-11-17 | Migration002 with exact narrow rights; UUID, all original columns/history/metadata preserved; zero health violations. |
| B7.4b runtime rehearsal | Exact runtime rights and rollback-only probe passed | Exact runtime rights and rollback-only probe passed | All nine table exports unchanged through probe; service-start/authentication interruptions remain in history. |
| B7.5/B7.6 development upgrade and final grants | Upgrade, reviewed verification completion, runtime transition and final verification passed, 2026-11-17 | Stages01–05 passed, 2026-11-17 | Former B7.6 is included in B7.5 stages04/05. Original migration preservation was verified BEFORE GUI edits; app19 table rights, diagnostic/migrator SELECT-only, schema2 and health verified. |
| B7.7 functional GUI | Owner accepted; broader own-record tests, with two evidence exceptions below | Owner explicitly accepted, 2026-11-20; hierarchy, ownership-moves, before/after restart and final checkpoints passed | Functional acceptance on both platforms stands. No inference that every Linux checkpoint or every individual manual/fault case passed. |
| B7.8 populated schema2 backup | Complete nine-table capture, before/after data/DDL equality, health, private review and final seal passed | Same; successful fresh capture/private review/seal after inspected ACL/validator failures | Sealed source provenance and all values including history/journals were subsequently checked during B7.9. Earlier incomplete Windows capture remains retained. |
| B7.9 exact schema2 restore | Stages01–05 passed; formally accepted, 2026-11-22 | Stages01–05 passed; owner confirms both complete | Exact canonical source/restored data and RAW DDL equality, nine tables/85 columns, UUID, completed journal/attempt, health, no extra stored code/cross-schema refs, final SELECT-only restore accounts. |
| B7.10 review/final acceptance | Review findings prepared here | Same review includes both platforms | Owner exception dispositions recorded in section6; explicit overall Phase1.2 acceptance remains pending. |

No outstanding migration/import attempt is reported uncertain at the completed
B7.5/B7.9 success endpoints. Earlier failures are preserved, not rewritten as
successful first attempts. In particular, Linux B7.5 migration was not replayed:
inspection established unchanged legacy bytes and the specific equivalent
LocalChange supporting-index name/position difference; reviewed completion
verified the already completed migration. B7.9 restores require strict raw DDL
equality with their own sealed snapshots, not that earlier alias allowance.

### Platform and artifact identity

| Identity | Linux | Windows |
| --- | --- | --- |
| Host / server | feledria / MariaDB10.5.29 | feldria-w10 / MariaDB13.0.2 |
| Desktop/build environment | CentOS Stream9 / Qt6.10.3 | Windows10 build19045 / MSYS2 UCRT64 / Qt6.12.0 |
| Captured native client / dump version | client15.1 / dump10.19 | client15.2 / dump10.20 |
| Dataset UUID | 21be9da7-b767-4578-b972-cc9a35122ddb | 457bb52b-fea4-4347-9c47-675c6b035c90 |
| Schema1 dump SHA256 | 83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26 | 6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50 |
| Sealed schema2 dump SHA256 | 81d6e16b45d141343ddad7dcb8700019f891af30f66ea01588a4e1d61d7ed0b4 | 32165acd3b9ae35c955eb8d4a92747b4c396b811ad70aedca946a774ed21ca0d |
| Original B7.8 package SHA256 | 46dbffef69d1d85eabbc9982a190dbff81466190a075810ab259200d3a4ed4b5 | f0203488fd1b019f8a9475d0c4ac41d940700550c658d3723831cf7e225291ad |
| B7.9 run package SHA256 | a276177955fe2e512e9d8222de70fd93f1463c364b19eb3b4ef308541f0d39ee | 9bb611c67037302cde6b501e45df0686ec0470629006eb21da7f94d4912c0f23 |

Schema2 snapshot paths:

- Linux: `/home/justin/temp/TaskCompassBackups/Phase1.2-schema2-linux-20261120-204710-103433`.
- Windows: `C:/Users/justin/TaskCompassBackups/Phase1.2-schema2-windows-20261121-222115-177176`.

B7.9 run paths:

- Linux: `/home/justin/temp/TaskCompassBackups/Phase1.2-restore-linux-20261121-234712-217a2dd622f3`.
- Windows: `C:/Users/justin/TaskCompassBackups/Phase1.2-restore-windows-20261122-023501-9a92eb740966`.

Both restored databases are CLI-only `taskcompass_phase12_restore`, with separate
same-named platform accounts at127.0.0.1, finally schema SELECT-only. GUI/database
allowlists are unchanged. B7.5 app/migration/diagnostic rights were not replaced
by these restore-account privileges. The two platforms intentionally retain
DIFFERENT dataset IDs; no synchronization or cross-platform identity merge occurred.

Counts in each sealed schema2 source, included in exact B7.9 comparison:

| Table | Linux | Windows |
| --- | ---: | ---: |
| SchemaMigration | 2 | 2 |
| Dataset | 1 | 1 |
| Task | 10 | 6 |
| CommandRecord | 77 | 91 |
| LocalChange | 77 | 91 |
| MigrationAttempt | 1 | 1 |
| Pillar | 4 | 6 |
| Project | 2 | 3 |
| Objective | 7 | 5 |

B7.9 operator package identities above come from the reviewed source/checksum
history associated with the platform runs; the private binding files themselves
were not independently read by the agent.

Preserve the incomplete Windows B7.8 capture ending
`20261121-192804-176636`, Windows B7.9 failed attempt directories ending
`20261122-015425-3b04fc2646b3` and `20261122-015453-9a89546ab38e`, and the
unsealed Linux B7.7 `final` export. They are historical failed artifacts, not
successful seals to repair or inputs to substitute for the approved snapshots.
All older Phase1.1/B7.2 backups and populated rehearsal databases remain retained;
this review performs no new private inventory audit or cleanup.

The snapshot records are dated evidence, not a fresh assertion about current dev
contents after possible later owner work. Final-manifest digests not included in
transcripts are not invented here; owner-reported seal/hash validations and the
private artifacts remain the source of that detail.

## 2. Linux exception E1 — protected task changed

**Status: OPEN; proposed disposition requires owner decision.**

Protected original Task UUID:
`24dd11c3-59df-4e23-ac21-422bcf2a1e3a`.
The B7.7 final checkpoint failed on protected-row inequality. Offline saved-export
inspection identified only `updatedAt`, `localRevision`, and `objectiveId` changed
for that protected task. The owner explained that Linux GUI testing used their
own tasks/organization rather than strictly the prescribed isolated QA records.
This supplies context for the deviation, not proof of a specific intended move.
The failed final checkpoint remains unsealed and must never be relabeled PASS.

Evidence supporting acceptance **with a documented variance**:

- B7.5 first verified original task/history preservation at migration time.
  This later GUI-time deviation does not negate that earlier upgrade result.
- A task ownership change normally updates ownership plus revision/timestamp;
  these three fields are consistent with such a command. That is an inference,
  not a reconstructed command/actor/timing or proof that no defect occurred.
- Later Linux B7.8 checks matched the retained final capture and identified this
  same limited original-row exception while preserving other protected values.
  Health and schema/Dataset checks passed. Both audit/outbox collections and the
  current task/organization state were captured and restored exactly in B7.9.
- No task deletion, invalid ownership graph or broader loss was established by
  the recorded comparison. No new such defect is reported by the owner.

What is **not** proven: that the specific `objectiveId` assignment was intended,
which exact GUI command/session made it, or whether the original task should now
remain at that owner. Matching a backup and restore preserves a changed value;
it does not establish the user's intent or prove the pre-QA value was unchanged.
Field-name-only inspection is not a full audit-history reconstruction.

**Recommendation:** accept this as a specifically documented owner-test/data-
isolation variance IF the owner confirms the changed task association is acceptable
as tested/current intended state, and explicitly accepts the three-field deviation.
Retain the historical failed preservation check, original baseline, later snapshot
and audit history. Do not repair/rebaseline/seal the failed B7.7 checkpoint.
This is not a recommendation to call its unchanged-original criterion passed.

Existing recorded machine evidence alone is insufficient for **unconditional**
closure of E1. The remaining gap is an owner-intent/acceptability decision, not
a missing migration or restore test. If the owner cannot accept the association,
first propose a narrowly scoped private review of saved snapshots/command history;
no new GUI writes, live inspection, restoration or testing is automatically needed
or authorized. No deliberate edit/reversal is performed to make a checksum pass.

## 3. Linux exception E2 — historical restart byte comparison missing

**Status: OPEN; proposed disposition requires owner decision.**

Linux missed its historical before/after GUI restart checkpoint pair. No later
result creates that absent time-specific byte comparison. The owner explicitly
confirmed on2026-11-22 that the Linux database remained intact after restarting
the application, and Linux GUI functional acceptance is already recorded.

Supporting evidence, kept distinct:

- Owner-observed Linux post-restart persistence is direct functional evidence.
- Existing [OrganizationDesktopTests](../tests/OrganizationDesktopTests.cpp) recreate the window/controller over shared
  in-memory storage and verify retained tasks, selected UUID and reset undo stack;
  archive-startup checks recreate the window too. These passed in offline suites.
  They are GUI/model lifecycle tests, not real database process-restart proof.
- Earlier native isolated integration results on both MariaDB versions include
  new-repository-connection persistence. Those are real database reconnection
  checks, not the missed Linux interactive GUI checkpoint pair.
- Windows has its own passed before/after GUI restart equality comparison.
  This supports cross-platform implementation confidence but does not substitute
  for a Linux byte comparison or prove the missing Linux interval.
- Linux B7.8/B7.9 prove later captured-state integrity and complete recoverability,
  including history, UUID, schema/journals and healthy ownership. They do not
  measure whether any value changed during that earlier restart interval.

**Recommendation:** existing evidence is sufficient for functional persistence
acceptance **with an explicit historical coverage exception**, subject to the
owner's decision. Do not require another restart merely to fill the old checklist:
it cannot recover the missed historical interval, and no new persistence defect
is identified in the supplied record. A new focused test would be justified only
by a specific remaining concern/acceptance requirement after this evidence review,
not automatically by the absent old artifact. No such new test is recommended now.

Residual uncertainty: exact byte-for-byte invariance of all rows during the
original Linux restart interval was not observed and cannot be asserted. Accepting
E2 would acknowledge that limitation; it would not turn the absent comparison
into a PASS. Leave it OPEN until the owner explicitly approves that disposition.

## 4. Other limitations retained, not new blanket retest requirements

- Manual GUI acceptance is owner-observed. Per-case checkboxes/fault experiments
  were not all separately supplied; do not invent case-by-case PASS records.
- Live GUI network-outage/permission-denial fault injection was intentionally not
  performed against valuable development data. Offline
  [injected unknown-commit tests](../tests/PersistentDesktopTests.cpp) and native
  isolated atomicity/lease-loss tests provide different coverage;
  they do not prove every live transport failure. These were documented limits.
- F5 recovery of a dead native connection remains unproven in the recorded GUI
  test evidence; normal close/relaunch was the documented fallback. No new fix
  or seamless reconnect guarantee is claimed by this review.
- Late GUI refinements (context-sensitive +, tree types/counts/sorting, archive
  startup reset, valid reparenting, compact rows) have recorded offline/native
  results and broad owner GUI acceptance. No new missing Phase1.2 feature or
  unresolved product defect is established by this documentation review.
- Schema2 backups/restore validation resolve the earlier *pending* backup/restore
  gates only. They do not waive the two Linux exceptions or final acceptance.

These limitations should remain in the acceptance record. Existing evidence is
adequate to propose accepting the approved milestone scope with those transparent
limits and the two specifically adjudicated exceptions. If stronger fault-case
or per-case evidence is demanded, identify that exact concern and obtain separate
scope before any inspection/test. Do not reopen all successful acceptance stages.

## 5. Decisions remaining before overall acceptance

1. **E1:** Does the owner accept the specific protected-task association/three-field
   deviation as an acceptable owner-test variance, without claiming untouched data?
2. **E2:** Does the owner accept functional persistence based on the accumulated
   evidence, with the historical Linux byte-comparison gap explicitly retained?
3. After both dispositions are recorded, review the remaining documented limits
   above and obtain **explicit final Phase1.2 acceptance**. Successful B7.9 and
   authorization to perform this review are not that acceptance.

E1 cannot be unconditionally closed from the record alone. E2 does not, in this
review, require redundant testing. Neither recommendation has been applied.
If the owner declines either proposed disposition, document a precise remaining
evidence need and request only the corresponding limited follow-up scope.

Preserve every database, account, QA record, backup, failed capture, original
seal and receipt. Later accepted-variance decisions must be appended to the
record, not backdated into old reports or used to rewrite checksums. No cleanup,
source/schema/grant changes, Phase1.3, commit/push/merge/tag or live action is
included. This review is documentation-only; no build/tests were repeated to
stand in for missing historical evidence.


## 6. Explicit owner dispositions — 2026-11-22

These decisions supersede the OPEN/pending language in the initial review above.
They do not alter a historical machine result, private artifact or checklist.

| Item | Owner decision | Historical result retained | Current disposition |
| --- | --- | --- | --- |
| E1: Task24dd11c3-59df-4e23-ac21-422bcf2a1e3a | Owner accepts updatedAt/localRevision/objectiveId variance as an acceptable result of their GUI testing and current data organization. | Original unchanged-record checkpoint FAILED; final capture remains unsealed. No untouched-row PASS or technical repair claimed. | OWNER-ACCEPTED VARIANCE; no outstanding owner-intent decision for this milestone. |
| E2: Linux GUI restart | Owner accepts functional persistence based on observed successful restart and supporting evidence. | Historical before/after all-row byte comparison NOT CAPTURED, never PASS. | OWNER-ACCEPTED HISTORICAL COVERAGE EXCEPTION. |

E1's owner acceptability/intent question is now explicitly adjudicated; no command-
by-command forensic reconstruction is claimed. E2's absent historical interval
remains unmeasured. These are accepted variances/limits, not repaired evidence.
No additional testing, reversal, rebaseline, artifact sealing or private history
inspection is required or recommended by these decisions. No earlier failed
capture, seal, manifest, task record or revision was modified.

## 7. Final Phase 1.2 acceptance summary and recommendation

**Recommendation ready for explicit owner approval; Phase 1.2 is NOT yet formally
accepted.** Both owner dispositions above have been recorded. The remaining
acceptance gate is the owner's explicit decision on the milestone as a whole,
with these dispositions and known limits attached.

Delivered approved scope:

- Persistent pillars/subpillars, projects/subprojects, objectives and independent/
  project/objective-owned tasks; validated ownership, nested navigation, moves,
  archive/restore and inherited archive filtering.
- Shared task/organization session undo/redo, command receipts/local-change history,
  transactional writes, revision/conflict/dependency guards and restart history
  reset. Session history is not promised to survive application restart.
- Owner-approved desktop refinements: context-sensitive +/right-click creation,
  valid selected-owner defaults, graphical entity types/tooltips, alphabetical
  groups, derived open-task counts, archive-off startup and compact task rows;
  validated organization reparenting, without Phase1.3 priority/manual ordering.
- Migration002 and cumulative nine-table/85-column schema, dataset identity,
  completed migration history/attempt journal, original migration-time preservation
  and final least-privilege development/runtime accounts on both platforms.
- Linux and Windows native build/offline evidence, earlier approved isolated
  database integration coverage, owner functional GUI acceptance, complete
  populated schema2 private backups/reviews/seals and exact isolated restores.
  Both restore accounts now SELECT-only; all populated databases/evidence retained.

Acceptance evidence is summarized in section1 and the chronological milestone;
live/manual execution remains owner-reported, offline checks remain separately
identified, and private records were not independently inspected by the agent.
No extra runtime checks are implied by this summary.

Known limitations and remaining coverage gaps carried into acceptance:

1. E1 remains a specific accepted three-field original-record variance, not
   unchanged-data verification. The original failed checkpoint remains visible.
2. E2's historical Linux restart byte comparison remains NOT CAPTURED; functional
   persistence is accepted with that limitation. A later test could not recreate
   the original missing interval, and no redundant restart is requested.
3. Manual per-case GUI observations are not exhaustively recorded. Broad explicit
   owner functional acceptance does not manufacture individual case PASS entries.
4. Valuable-dev GUI network-outage/permission-denial fault injection was not run.
   Offline unknown-commit and native isolated lease/atomicity tests provide useful
   but different coverage; not every real transport/crash path is proven.
5. F5 seamless recovery of a dead native connection remains unproven in the record;
   normal close/relaunch is the documented fallback. No automatic recovery fix
   or stronger reconnect guarantee is claimed.
6. Checks establish the states at their recorded times, not continuous monitoring
   of later owner activity. Original package provenance and failed artifacts
   remain historical; private-file audits not performed by the agent are not
   implied. Older Phase1.1 snapshots and all rehearsal/restore/QA data stay retained.

No new unresolved in-scope product defect or additional mandatory execution gate
was identified by this record-only reconciliation. This supports recommending
acceptance of the approved scope WITH the transparent limitations above, rather
than claiming exhaustive testing or retroactive byte-for-byte evidence.

**Final Acceptance Approval Requested:** accept TaskCompass Phase1.2 as delivered
on Linux and Windows, with E1/E2 owner decisions and the known limitations/coverage
gaps preserved in the acceptance record. This request is not a declaration that
approval has been granted. Until the owner explicitly decides, status remains
AWAITING FINAL OWNER ACCEPTANCE.

Any future enhancement/fault test needs its own scope. Final acceptance would
not authorize cleanup, account/database removal, evidence rewriting, Git commit/
push/merge/tag or Phase1.3. No code/schema/privilege/data changes, live operations
or additional test execution were performed to prepare this summary.
