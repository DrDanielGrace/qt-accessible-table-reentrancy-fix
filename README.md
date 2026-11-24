# Qt accessibility re-entrancy fixes

This repository publishes two related Qt accessibility correctness fixes found
while debugging a macOS ARM64 Qt Widgets application (LabPlot 2.12.80) using
Qt 6.12.0.

## 1. QAccessibleCache deletion re-entrancy

`QAccessibleCache::deleteInterface()` used to hold an iterator into
`idToInterface` across a synchronous `ObjectDestroyed` event. Re-entrant
platform callbacks could delete/rehash the same entry, leaving the outer call
with a stale iterator and a double deletion. The Apple element cache had the
same invalidate/release ordering problem.

This is the already-published Qt fix:

- commit `1d3a303846c2df629b9b7d3e4e4d87c4f61272b4`
- 6.12 cherry-pick `a9807578c4b4`
- `QTBUG-149629`

The patch is included here for completeness:
`patches/qt-6.12.0-accessible-cache-reentrancy.patch`.

## 2. QAccessibleTable::modelChange re-entrancy

The second crash was symbolicated with the exact KDE Craft Qt 6.12.0 dSYM:

```text
QAccessibleTable::modelChange(QAccessibleTableModelChangeEvent*)
    (itemviews.cpp:696)
```

`modelChange()` walks `childToId` while deleting accessible interfaces.
`deleteAccessibleInterface()` sends `ObjectDestroyed` synchronously. The
platform accessibility backend can re-enter `QAccessibleTable::child()` and
insert into `childToId`, invalidating the iterator and corrupting the cache.

The proposed correction snapshots the old cache, clears the live map before
deleting, iterates the immutable snapshot, skips interfaces already deleted
re-entrantly, and null-checks the parent lookup.

Proposed patch:
`patches/qt-6.12.0-accessible-table-reentrancy.patch`.

Branches in the fork:

- dev based: <https://github.com/DrDanielGrace/qtbase/tree/a11y-modelchange-reentrancy>
- 6.12 based: <https://github.com/DrDanielGrace/qtbase/tree/a11y-modelchange-reentrancy-6.12>

## Why there is no pull request against qt/qtbase

`qt/qtbase` on GitHub is a mirror. Its GitHub metadata has `has_issues=false`,
and the GitHub API `/repos/qt/qtbase/pulls` endpoint returns HTTP 404. Qt's
`CONTRIBUTING.md` states that GitHub pull requests are not accepted and that
all changes must go through <https://codereview.qt-project.org>.

The fork branches above are ready to be pushed to Gerrit. This repository keeps
the reviewable patch, reproducers and evidence public in the meantime.

## Reproducers

```sh
cmake -S tests -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/macos       -DACCESSIBLE_REPRO_ASAN=ON
cmake --build build -j
./build/cache_reentry
./build/itemview_reentry remove
./build/itemview_reentry reset
./build/itemview_reentry destroy
./build/treeview_reentry
```

`treeview_reentry` exercises `QAccessibleTable::modelChange()` with a real
`QTreeView`, a fictional model, and a re-entrant accessibility update handler.

## Test status

Against official Qt 6.12.0 and 6.12.1, the cache/item-view tests crash with an
ASan SEGV in `QAccessibleCache::deleteInterface()`.

Against a feature-matched Qt 6.12.0 build containing both fixes, all five tests
pass. A full LabPlot 2.12.80 build against that Qt launches, imports fictional
CSV data, creates curves, and exports PNG/PDF/SVG.

Detailed reports:

- `docs/NEW-CRASH-SYMBOLICATION.md`
- `docs/BUILD-COMPATIBILITY.md`
- `docs/FINAL-REPORT.md`

## Scope and caution

The second patch is based on a real crash and exact symbolication, but no
minimal upstream Qt-only test has yet been made to fail deterministically on
the old code. It should be reviewed as a hardening fix for iterator/cache
mutations across synchronous accessibility callbacks.

The patch is provided under the same Qt licensing terms as the modified files.
The standalone test programs are provided under the MIT license.
