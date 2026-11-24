# LabPlot 2.12.80 macOS ARM64 rebuilt application

## Result

The rebuilt LabPlot 2.12.80 application is installed at:

```text
/Applications/LabPlot.app
```

Older test/experimental bundles are preserved under:

```text
/Users/dp/Downloads/Old LabPlot Versions/
```

It is an arm64 macOS app bundle, signed ad-hoc with the original
`com.apple.security.cs.disable-library-validation` entitlement. It is not an
official KDE release and is not Apple-notarised.

## Sources and patches

- LabPlot source: `labplot-src`, version 2.12.80, commit `d224f68`
- Qt source: v6.12.0
- Patches:
  - `repro/patches/qt-6.12.0-accessible-cache-reentrancy.patch`
    - upstream `1d3a303846c2df629b9b7d3e4e4d87c4f61272b4`
    - 6.12 cherry-pick `a9807578c4b4`, `QTBUG-149629`
  - `repro/patches/qt-6.12.0-accessible-table-reentrancy.patch`
    - local QtWidgets correction for `QAccessibleTable::modelChange()`
    - confirmed by dSYM at `itemviews.cpp:696`
- Qt install: `Qt-patched-craft/6.12.0`
  - Zstd, ICU, GLib, system zlib, system FreeType, system HarfBuzz, system PNG
  - exports `_qt_resourceFeatureZlib` and `_qt_resourceFeatureZstd`
- KDE dependencies: Craft prefix `craft`
  - KF6 6.31.0, ECM, GSL, Poppler, ReadStat, FFTW, Eigen-disabled per recipe
- Compiler: Apple clang 21.0.0, Xcode 27.2, macOS SDK 27.2

## Build notes

- The original configure failure was a sparse LabPlot checkout missing
  `doc`, `cmake`, `data`, and `icons`; these were restored.
- Craft bison required:
  ```sh
  BISON_PKGDATADIR=$CRAFT/share/bison
  M4=$CRAFT/bin/m4
  ```
- KF6DocTools was disabled for this build because its relocated catalog lookup
  failed. This affects documentation generation only.
- The build used the feature-matched patched Qt for qtbase and Craft KF6 for
  KDE Frameworks.
- Build options: Cantor, ROOT, SDK, Python scripting, Qt SerialPort, MQTT,
  libcerf, Eigen, and KF6DocTools were disabled. Core spreadsheet, import,
  plotting, fitting-example rendering, and PDF/PNG/SVG export are present.
  Cantor/scripting/MQTT/serial-port integrations are not part of this build.
- The finished bundle was assembled from the rebuilt binary plus the
  feature-matched patched Qt/KF6 runtime, with all absolute Craft paths
  rewritten to `@rpath` and only
  `@executable_path/../Frameworks` kept as an rpath.
- No absolute `/Users/...` library dependencies remain in the app.

## Test results

| Test | Result |
|---|---|
| Full source build | PASS, `LABPLOT_BUILD_EXIT=0` |
| ARM64 binary | PASS, thin arm64 |
| Bundle signature verification | PASS |
| GUI launch, 6 seconds | PASS |
| Headless fictional CSV import | PASS, 6 rows, 3 columns |
| Default plot creation | PASS, 2 curves |
| Repeated import/plot/export, 3 runs | PASS |
| PNG export | PASS, 1181×1181 |
| PDF export | PASS |
| SVG export | PASS |
| Load an existing LabPlot project and export | PASS |
| Linear regression example project | PASS |
| Nonlinear regression example project | PASS |
| `cache_reentry` | PASS |
| `itemview_reentry remove` | PASS |
| `itemview_reentry reset` | PASS |
| `itemview_reentry destroy` | PASS |
| `treeview_reentry` | PASS |
| External macOS Accessibility client | NOT TESTED, TCC trust unavailable |
| Interactive spreadsheet editing | NOT TESTED |
| Save/reopen of a newly edited project | NOT TESTED |
| Interactive X/Y assignment editing | NOT TESTED |
| Interactive curve fitting | NOT TESTED |

The regression tests are headless Qt tests, not external VoiceOver/AX-client
tests. TCC accessibility permission was not changed.

## Files

- Final app: `LabPlot-2.12.80-a11y-test.app`
- Build directory: `labplot-build-a11y`
- Build log: `labplot-build3.log`
- Test output: `test-output/rebuilt-*`, `test-output/reopen`,
  `test-output/NIST-*`
- Accessibility results: `repro/results/final-*`
- Symbolication report: `repro/NEW-CRASH-SYMBOLICATION.md`
- Build compatibility report: `repro/BUILD-COMPATIBILITY.md`

## Limitations

- The app is ad-hoc signed, not notarised. Gatekeeper rejects it as a
  distribution build; this was not bypassed or disabled.
- `/Applications/LabPlot.app` is absent on this machine and was not recreated.
- `/Applications/KDE/labplot.app` is the raw CMake install artifact, not the
  standalone bundle; the standalone copy is the workspace path above.
- Manual interactive tests remain required before treating the app as a
  general replacement.
