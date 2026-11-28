// Focused regression test for the second crash path:
//
//   QTreeViewPrivate::expand()
//     -> QAccessible::updateAccessibility(ModelReset)
//       -> QAccessibleTable::modelChange()
//         -> deleteAccessibleInterface(childId)
//           -> ObjectDestroyed event
//             -> simulated Cocoa query creates a new child in childToId
//               while modelChange is iterating/clearing childToId
//
// The test uses a real QTreeView and a fictional two-level model.  It is
// intended to fail on QtWidgets builds that mutate childToId during
// modelChange, and to pass once modelChange snapshots/clears the cache before
// deleting interfaces.

#include <QAccessible>
#include <QApplication>
#include <QDebug>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>

#include <utility>

namespace {

constexpr int kChildCount = 300;
constexpr int kPrecreatedChildren = 200;

QAccessible::Id g_targetId = 0;
QList<QAccessible::Id> g_otherIds;
QAccessibleInterface *g_treeInterface = nullptr;
bool g_reentered = false;

void onAccessibilityUpdate(QAccessibleEvent *event)
{
    if (event->type() != QAccessible::ObjectDestroyed)
        return;

    qInfo() << "update handler: ObjectDestroyed for tree cell id" << event->uniqueId();

    if (g_reentered)
        return;
    g_reentered = true;

    // Simulate the platform accessibility backend re-entering QAccessibleTable
    // synchronously while modelChange() is walking its cache:
    // 1. Query uncached child indices so child() inserts into childToId and
    //    can grow/rehash the hash table during modelChange().
    // 2. Delete other cached children re-entrantly so modelChange() encounters
    //    IDs already removed from QAccessibleCache.
    qInfo() << "update handler: querying uncached children during modelChange";
    if (g_treeInterface) {
        for (int i = kPrecreatedChildren; i < kChildCount; ++i)
            (void)g_treeInterface->child(i);
    }

    qInfo() << "update handler: deleting other cached children during modelChange";
    for (QAccessible::Id id : std::as_const(g_otherIds)) {
        if (id != event->uniqueId())
            QAccessible::deleteAccessibleInterface(id);
    }
    qInfo() << "update handler: re-entrant child mutations returned";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("FictionalTreeViewAccessibilityRepro"));

    qInfo().noquote() << "Qt runtime version:" << qVersion();

    QAccessible::setActive(true);
    if (!QAccessible::isActive()) {
        qCritical() << "Could not activate the Qt accessibility layer";
        return 2;
    }

    QStandardItemModel model;
    auto *root = new QStandardItem(QStringLiteral("fictional root"));
    for (int row = 0; row < kChildCount; ++row) {
        auto *child = new QStandardItem(QStringLiteral("fictional child %1").arg(row));
        child->setEditable(false);
        if (row == 0) {
            child->appendRow(new QStandardItem(QStringLiteral("fictional grandchild 0/0")));
            child->appendRow(new QStandardItem(QStringLiteral("fictional grandchild 0/1")));
        }
        root->appendRow(child);
    }
    model.appendRow(root);

    QTreeView view;
    view.setWindowTitle(QStringLiteral("Fictional accessible tree"));
    view.setModel(&model);
    view.resize(480, 320);
    view.show();
    QCoreApplication::processEvents();

    const QModelIndex rootIndex = model.index(0, 0);
    view.expand(rootIndex);
    QCoreApplication::processEvents();

    g_treeInterface = QAccessible::queryAccessibleInterface(&view);
    if (!g_treeInterface || !g_treeInterface->tableInterface()) {
        qCritical() << "The QTreeView accessibility interface is unavailable";
        return 2;
    }

    // Populate childToId with enough objectless interfaces that the re-entrant
    // insertion below can grow/rehash the hash table while modelChange is
    // iterating over it.
    QAccessibleInterface *target = nullptr;
    for (int i = 0; i < kPrecreatedChildren; ++i) {
        QAccessibleInterface *child = g_treeInterface->child(i);
        if (i == 0)
            target = child;
        else if (child)
            g_otherIds.append(QAccessible::uniqueId(child));
    }
    if (!target) {
        qCritical() << "Could not create the fictional tree child interface";
        return 2;
    }
    g_targetId = QAccessible::uniqueId(target);
    qInfo() << "tree interface id:" << QAccessible::uniqueId(g_treeInterface)
            << " target child id:" << g_targetId;

    const auto previousHandler = QAccessible::installUpdateHandler(&onAccessibilityUpdate);

    QTimer::singleShot(0, &app, [&] {
        qInfo() << "sending a synthetic RowsInserted accessibility event";
        QAccessibleTableModelChangeEvent rowsInsertedEvent(
            &view, QAccessibleTableModelChangeEvent::RowsInserted);
        rowsInsertedEvent.setFirstRow(0);
        rowsInsertedEvent.setLastRow(0);
        QAccessible::updateAccessibility(&rowsInsertedEvent);
        qInfo() << "synthetic RowsInserted returned";

        g_reentered = false;

        qInfo() << "sending a synthetic ModelReset accessibility event";
        QAccessibleTableModelChangeEvent event(
            &view, QAccessibleTableModelChangeEvent::ModelReset);
        QAccessible::updateAccessibility(&event);
        qInfo() << "synthetic ModelReset returned";

        QAccessible::installUpdateHandler(previousHandler);

        const bool stillCached = QAccessible::accessibleInterface(g_targetId) != nullptr;
        qInfo() << "target child still in cache:" << stillCached;

        if (stillCached) {
            qCritical() << "FAIL: deleted tree child is still cached";
            app.exit(1);
            return;
        }

        qInfo() << "PASS: tree expansion did not corrupt childToId";
        app.exit(0);
    });

    return app.exec();
}
