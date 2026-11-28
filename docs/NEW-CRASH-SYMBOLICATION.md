# New tree-view crash: symbolication and root cause

## Exact symbolication

The latest crash reported the unsymbolicated QtWidgets frame at image offset
`2747652` (`0x29EE44`).

The original LabPlot bundle's QtWidgets UUID is:

```text
83C509D0-0EE3-3790-8B47-55F70DAF9CAD
```

KDE's Craft binary cache contains a Qt 6.12.0 build with the **same QtCore and
QtWidgets UUIDs** as the original LabPlot bundle, plus its dSYM package:

```text
QtCore:    C368EFD9-1CEC-3349-A05D-6EBD1ECA9FD5
QtWidgets: 83C509D0-0EE3-3790-8B47-55F70DAF9CAD
```

This identifies the exact Qt build used by LabPlot. Its configure log shows
`FEATURE_zstd`, `FEATURE_icu`, `FEATURE_glib`, `FEATURE_system_zlib`,
`FEATURE_system_freetype`, `FEATURE_system_harfbuzz`, and
`FEATURE_system_png` (via the Craft dependency set).

Using the matching dSYM:

```sh
atos -o QtWidgets.dSYM/Contents/Resources/DWARF/QtWidgets \
     -arch arm64 -l 0x0 0x29ee44
```

returns:

```text
QAccessibleTable::modelChange(QAccessibleTableModelChangeEvent*)
    (in QtWidgets) (itemviews.cpp:696)
```

## What line 696 is

In the unmodified v6.12.0 source, line 696 is the closing brace of the
`RowsInserted` / `ColumnsInserted` branch of
`QAccessibleTable::modelChange()`:

```cpp
ChildCache newCache;
ChildCache::ConstIterator iter = childToId.constBegin();

while (iter != childToId.constEnd()) {
    QAccessible::Id id = iter.value();
    QAccessibleInterface *iface = QAccessible::accessibleInterface(id);
    ...
    if (indexOfChild(iface) >= 0) {
        newCache.insert(indexOfChild(iface), id);
    } else {
        QAccessible::deleteAccessibleInterface(id);
    }
    ++iter;
}
childToId = newCache;
```

The crash occurs while the local cache is being cleaned up/assigned after the
loop. The loop holds a `const_iterator` into `childToId` while it calls
`deleteAccessibleInterface()`, which synchronously sends an `ObjectDestroyed`
event. The Cocoa accessibility backend can re-enter the table/tree and call
`child()`, which inserts into `childToId`. That mutates the container while the
iterator is still in use. A related problem exists in the `ModelReset` branch,
which iterates `childToId` while deleting from it.

This is a **separate QtWidgets defect** from the already-fixed
`QAccessibleCache::deleteInterface()` iterator bug. It explains why the patched
QtGui alone still crashes in QtWidgets.

## Local correction

`patches/qt-6.12.0-accessible-table-reentrancy.patch`:

- snapshots `childToId` and clears the live map before deleting interfaces;
- iterates the immutable snapshot, never the live hash;
- skips ids whose interface was already deleted re-entrantly;
- checks `iface` and `parent` before dereferencing them;
- writes rebuilt entries directly into `childToId` instead of assigning over
  any interfaces created by re-entrant callbacks.

## Regression test

`tests/treeview_reentry.cpp` creates a real `QTreeView` with fictional
children, populates the accessible child cache, and sends a synthetic
`ModelReset` accessibility event. During `ObjectDestroyed`, it deletes other
cached children re-entrantly, exactly the kind of cache mutation that caused
the invalid cleanup.

The test passes against the rebuilt QtWidgets and is included in the normal
build. It has not yet been possible to run it against an external macOS
Accessibility client because this process is not trusted by TCC and the
instructions prohibit changing that setting.

## Remaining validation

The exact original LabPlot executable cannot be made to use the locally built
QtCore because that QtCore was built without Zstd/ICU/GLib. That is a
build-feature mismatch, not an Accessibility result. A fresh LabPlot build
against a feature-matched, patched Qt is required before the application-level
crash can be declared fixed.
