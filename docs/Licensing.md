# License and redistribution review — Phase 0.2

Review date: 2026-11-10. SPDX-License-Identifier: GPL-3.0-or-later.

The owner selected **GPL-3.0-or-later** for project-owned TaskCompass code and
documentation. [LICENSE](../LICENSE) contains the unmodified standard GNU GPL
version 3 text, including its application appendix. The README grants the choice
of version 3 or any later version. There is no separate “or-later” license body;
that choice is made by the grant/identifier, as explained by
[SPDX GPL-3.0-or-later](https://spdx.org/licenses/GPL-3.0-or-later.html).
No per-source copyright headers, owner identity guesses, exceptions or dependency
license changes were introduced.

No blocker was identified to adopting this license for the current desktop source
and architecture. This is a scoped compatibility assessment, not clearance of every
future binary package or distribution channel. Native iOS distribution remains an
unresolved release issue, not a reason to silently choose another project license.

## Current components and evidence

CMake uses Qt Core/Gui/Widgets/SQL, and Qt Test for tests. QMYSQL dynamically loads
MariaDB Connector/C on the inspected Linux machine. There are no vendored libraries,
icons, fonts or sample-data packages in the repository. Compiler/build tools are
not imported as application code merely by building with them.

| Component | Evidence / assessment |
| --- | --- |
| Qt 6.10.3 / QMYSQL | Installed RPM metadata: `LGPL-3.0-only OR GPL-3.0-only WITH Qt-GPL-exception-1.0`; Qt package supplies GPLv3 and LGPL texts. A GPLv3 project can comply with an available compatible Qt route. Preserve module-specific terms; Qt itself is not relicensed “or later” by this project's README. |
| MariaDB Connector/C 3.2.6 | Installed RPM reports LGPLv2+ and COPYING.LIB contains LGPL 2.1. Dynamic linking to this LGPL library is compatible with a GPLv3 application when the relevant obligations are met. This is the inspected MariaDB connector, not a blanket conclusion about every MySQL client library. |
| MariaDB server | Separate database process, not linked into the GUI. Merely connecting does not license the user's data under GPL. If a server binary is bundled, review that binary's separate license/source notices too. |
| GCC/libstdc++ | RPM metadata includes GPL with runtime exceptions and other component terms. Check the actual runtime-exception and bundled-library notices for release artifacts, especially Windows compiler runtimes. Building does not automatically license all output under the compiler's own license. |
| CMake / Ninja | Installed metadata records permissive component licenses (CMake includes BSD/MIT/Zlib/Apache; Ninja Apache-2.0). Usually development-only; preserve their notices if distributing the tools themselves. |
| Transitive libraries/plugins | Not exhaustively cleared here. Qt lists third-party code and supports version-specific SBOMs; inspect actual deployed plugin/runtime contents before a release. |

Sources: [Qt licensing](https://doc.qt.io/qt-6/licensing.html) explains LGPL/GPL
options and separate third-party components. [Qt open-source obligations](https://www.qt.io/development/open-source-lgpl-obligations)
cover notices, source, user modification/relinking rights and distribution-channel
restrictions. Local package evidence is reproducible with:

```bash
rpm -q --qf '%{NAME}: %{LICENSE}\n' qt6-qtbase qt6-qtbase-mysql mariadb-connector-c gcc libstdc++ cmake ninja-build
```

The standard license body was copied byte-for-byte from the installed
`/usr/share/licenses/qt6-qtbase/GPL-3.0-only.txt`. Its filename describes Qt's grant,
not the different grant made for TaskCompass. GPL license text itself permits
verbatim copying. The project's selection is unambiguously **GPL-3.0-or-later**.
GNU web retrieval timed out during this review; the local standard text and SPDX
license reference were available. No license text was reconstructed or shortened.

## Linux and Windows redistribution

Recommended initial route: GPLv3-compatible open-source distribution with matching
source/build instructions and dynamically linked Qt/connector libraries. Both
Linux packages and Windows archives/installers must preserve copyright/license
notices, disclaimers and third-party terms; provide Corresponding Source by a
GPL-compliant method for the conveyed binaries, including required scripts and
modifications. Do not assume a repository link alone always satisfies every binary
conveyance scenario. Use the actual licenses in shipped versions to settle scope.

If distributing under an LGPL library route, also meet its modification/replacement/
relinking and source requirements; static linking needs particular attention.
GPL-compatible licensing does not erase those obligations. Record exactly which
Qt licensing route is used and include its notices. Do not claim a commercial Qt
license is necessary merely because the desktop runs on Windows.

Add an About/licenses display and release notice bundle before distributing a
production application. No prototype GUI changes are required in this phase.
Build an artifact-specific dependency inventory/SBOM at release time; development
RPM metadata is evidence for this host, not a complete Windows redistribution bill.

## Native iPhone distribution — unresolved

Evaluate the actual Qt/mobile modules, linkage, signing/provisioning model, current
Apple store terms and DRM/restriction behavior before choosing a distribution
channel. A user's rights to modify/relink/install and restrictions on conveying GPL
software can conflict with a locked-down app store or deployment arrangement.
Qt's obligations page specifically warns about incompatible store rules. Source
availability by itself does not resolve all such conflicts.

Do not treat successful Xcode compilation, sideloading or an App Store submission
as proof of license compliance. Distinguish personal device experiments from
conveying binaries to others. A commercial Qt license could address Qt-specific
terms, but **does not remove TaskCompass's GPL obligations**, including those of
shared GPL C++ core code. Replacing QML with another UI also does not automatically
solve the application's license/channel constraints.

Before Phase 6 distribution, obtain an assessment of the chosen route and the
rights in all included contributions. Any alternate license, store exception,
dual-license grant or contributor agreement needs explicit owner approval and the
necessary rights from relevant copyright holders. None is authorized or added by
this review. Keep the selected GPL-3.0-or-later license unchanged in the meantime.
