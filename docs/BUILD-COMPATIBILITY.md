# QtCore build-feature compatibility report

## Observation

`LabPlot-consistent-test.app` aborts before `main()` with:

```text
Symbol not found: _qt_resourceFeatureZstd
Referenced from: LabPlot.app/Contents/MacOS/labplot
Expected in:     QtCore.framework/Versions/A/QtCore
```

This is a dynamic-linker/build-configuration failure, not an Accessibility
crash.

## Original LabPlot QtCore

The QtCore bundled with the original LabPlot application:

- UUID: `C368EFD9-1CEC-3349-A05D-6EBD1ECA9FD5`
- links ICU 78, GLib, bundled zlib 1.3.1, and bundled zstd 1.5.7
- exports both resource feature symbols:

```text
_qt_resourceFeatureZlib
_qt_resourceFeatureZstd
```

The original QtGui additionally links bundled PNG, FreeType, HarfBuzz, and
GThread libraries.

## Locally built patched QtCore

The QtCore built from the v6.12.0 tag with the Accessibility patches:

- UUID: `5E15A1A8-9BEE-3735-AC02-49B69C4C60A4`
- uses the system zlib
- has no ICU, GLib, or Zstd support
- exports only:

```text
_qt_resourceFeatureZlib
```

Its configuration summary confirms:

```text
Using system zlib ...................... yes
GLib ................................... no
ICU .................................... no
```

## Consequence

The original LabPlot executable was compiled by its original Qt build to use
Zstd-compressed Qt resources. The feature symbol is part of the QtCore ABI
surface generated from that build configuration. Replacing QtCore with a
different-feature build is not a supported framework swap, even though both
report Qt 6.12.0.

Adding a fake `_qt_resourceFeatureZstd` would hide the loader error while
leaving the rest of the QtCore feature mismatch unresolved. It is not an
acceptable fix.

## Exact original Qt build

KDE's Craft cache contains a Qt 6.12.0 build whose QtCore and QtWidgets UUIDs
match the original LabPlot bundle exactly. Its configure log shows these
features were enabled:

```text
FEATURE_zstd=ON
FEATURE_icu=ON
FEATURE_glib=ON
FEATURE_system_zlib=ON
FEATURE_system_freetype=ON
FEATURE_system_harfbuzz=ON
FEATURE_dbus=ON
FEATURE_openssl_linked=ON
```

That is why the original executable expects `_qt_resourceFeatureZstd`. The
local patched QtCore was configured without Zstd, ICU, or GLib and is therefore
not a valid replacement for the existing executable.

## Correct direction

Rebuild LabPlot against the patched Qt environment so that LabPlot,
QtCore/QtGui/QtWidgets, its Qt plugins, and KDE Frameworks are all compiled
against the same Qt configuration.

Two workable approaches:

1. **Preferred:** build ECM, KF6, GSL, and LabPlot against the patched Qt.
   This produces a coherent application and avoids trying to make a foreign
   executable accept a differently configured QtCore.
2. Build Qt with the original application's full feature set (Zstd, ICU,
   GLib, bundled PNG/FreeType/HarfBuzz, OpenSSL, etc.) and replace the whole
   Qt stack. This is more work and still risks mismatches with KF6 modules
   built against the original Qt build.

The first approach is the one being pursued. Craft was bootstrapped with a
workspace-local Python 3.12 runtime because the system Python 3.9 is too old.
A binary-cache dependency install is in progress; if a dependency is not in
the cache it may still need to be built from source.

## Security and preservation

- No macOS TCC, Gatekeeper, SIP, or library-validation setting was changed.
- The original application was not intentionally modified.
- All test bundles remain separate from `/Applications`.
- `/Applications/LabPlot.app` was present at the start of the investigation but
  is currently absent from the machine; this is recorded as an external change,
  not recreated or overwritten.
