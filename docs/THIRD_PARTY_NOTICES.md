# Licensing, attribution and distribution

Reviewed against main at `82794d04b06d60d09bfa22722b803d4ec16d18b5` on 2026-10-31.

## Project license and provenance

The source headers and README specify **GPL-2.0-or-later**. The root [LICENSE](../LICENSE) contains the GPL version 2 text; the "or later" permission comes from the project's notices, not GitHub's automatic license badge. Retain copyright/license notices and comply with the applicable GPL version when modifying or distributing covered work. Binary distribution also requires satisfying its corresponding-source obligations; a binary download alone is not a complete source delivery.

The project is based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate), as recorded in [DEVELOPMENT.md](DEVELOPMENT.md). Template-derived build support and helper files retain upstream terms. The exact template revision and complete mapping of inherited files are not recorded. The placeholder copyright fields in `src/plugin-support.h` and `src/plugin-support.c.in` are unresolved template metadata; do not replace them with an invented author or year.

## Dependencies and build tools

| Component | Evidence and role | Licensing scope |
| --- | --- | --- |
| OBS Studio, libobs and frontend API | Linked in [CMakeLists.txt](../CMakeLists.txt). Windows source version 31.1.1 is hash-pinned in [buildspec.json](../buildspec.json). | OBS has its own [GPL notices and source](https://github.com/obsproject/obs-studio/tree/31.1.1); preserve applicable upstream notices. |
| Qt 6 Core and Widgets | Linked in CMake. Windows uses the OBS-provided Qt dependency archive dated 2025-07-11. | [Qt offers multiple licensing options](https://doc.qt.io/qt-6/licensing.html). Use the terms and third-party notices for the actual modules and build. The project's GPL-2.0-or-later notice does not establish that every Qt combination can be distributed under GPLv2 alone. |
| OBS prebuilt dependencies | Separate hash-pinned archive in buildspec.json. | A bundle may contain components under several licenses; the plugin license does not relicense that archive. Retain its component notices if redistributing any of it. |
| Linux OBS/Qt and Flatpak runtime/SDK | Native Linux uses installed packages; Flatpak derives its SDK from the installed stable OBS runtime. | These versions can vary by environment. Review the supplying distribution/runtime's licenses and exact dependency inventory. |
| CMake, compiler/SDK, Ninja, Python, Flatpak and formatting/CI tools | Build/development tools described in DEVELOPMENT.md and scripts. | Tool licenses remain separate; their use does not grant rights to redistribute toolchains or bundled runtimes. |

## Assets and attribution

`media/*.png` are documentation screenshots. DEVELOPMENT.md and `scripts/capture-screenshots.cpp` describe rendering the real Qt plugin widgets with demonstration data. This supports screenshot provenance, not a separate grant for any upstream theme, font, logo, or other material appearing in an image. No standalone media license is recorded. OBS and Qt names identify dependencies; no endorsement or trademark rights are granted here.

## Release scope and remaining checks

The v1.4.0 Windows ZIP and native Linux archive were inspected without executing them. Both include LICENSE and the plugin, and neither contains a bundled OBS/Qt runtime. Linux BUILD-INFO.txt records OBS 32.2.0, Qt 6.4.2 and the shared-library inventory; it is specific to that build. All three v1.4.0 package hashes matched the published SHA256SUMS.txt. The Flatpak checksum was verified, but its internal OSTree payload and notices were not inspected in this Windows audit.

For a release, retain the exact source revision, build scripts, dependency versions/notices and any required source for redistributed dependencies. [v1.4.0 source](https://github.com/technicallyalex/recording-folder-dock/tree/v1.4.0) and the repository build instructions are available, but this audit did not reproduce the binaries or certify a complete corresponding-source delivery. Full template attribution, dependency redistribution terms and Flatpak payload coverage remain to be verified before claiming complete compliance.
