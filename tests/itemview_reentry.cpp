// Reproducer using the same path as a real LabPlot QTableView:
//
//   QAbstractItemViewPrivate::rowsRemoved()  (or reset()/view destruction)
//     -> QAccessible::updateAccessibility(TableModelChanged)
//       -> QAccessibleTable::modelChange()
//         -> QAccessible::deleteAccessibleInterface(cellId)
//           -> QAccessibleCache::deleteInterface(cellId)
//             -> ObjectDestroyed event
//               -> simulated platform re-entry
//
// QAccessibleTableCell::object() returns nullptr (see Qt's itemviews_p.h), so
// deleting a cell interface takes the event-sending branch in
// QAccessibleCache::deleteInterface().
//
// Usage:
//   itemview_reentry [remove|reset|destroy]    (default: remove)
//
// Expected result:
//   Qt 6.12.0 (unpatched): crash / ASan heap-use-after-free / double delete.
//   Qt with QTBUG-149629 fix: prints PASS and exits 0.

#include <QAccessible>
#include <QApplication>
#include <QDebug>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>

namespace {

QAccessible::Id g_targetId = 0;
bool g_reentered = false;
int g_destructorCount = 0;

void onAccessibilityUpdate(QAccessibleEvent *event)
{
    if (event->type() != QAccessible::ObjectDestroyed
        || event->uniqueId() != g_targetId) {
        return;
    }

    qInfo() << "update handler: ObjectDestroyed for table cell id" << event->uniqueId();

    if (g_reentered)
        return;
    g_reentered = true;

    qInfo() << "update handler: re-entering deleteAccessibleInterface for cell";
    QAccessible::deleteAccessibleInterface(g_targetId);
    qInfo() << "update handler: nested deleteAccessibleInterface returned";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("FictionalItemViewAccessibleRepro"));

    const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                  : QStringLiteral("remove");
    if (mode != QLatin1String("remove")
        && mode != QLatin1String("reset")
        && mode != QLatin1String("destroy")) {
        qCritical() << "Usage:" << argv[0] << "[remove|reset|destroy]";
        return 2;
    }

    qInfo().noquote() << "Qt runtime version:" << qVersion();
    qInfo() << "mode:" << mode;

    QAccessible::setActive(true);
    if (!QAccessible::isActive()) {
        qCritical() << "Could not activate the Qt accessibility layer";
        return 2;
    }

    QStandardItemModel model(6, 3);
    for (int row = 0; row < model.rowCount(); ++row) {
        for (int column = 0; column < model.columnCount(); ++column) {
            model.setItem(row, column,
                          new QStandardItem(QStringLiteral("fictional %1/%2")
                                                .arg(row)
                                                .arg(column)));
        }
    }

    auto *view = new QTableView;
    view->setWindowTitle(QStringLiteral("Fictional accessible table"));
    view->setModel(&model);
    view->resize(640, 400);
    view->show();
    QCoreApplication::processEvents();

    QAccessibleInterface *viewInterface = QAccessible::queryAccessibleInterface(view);
    if (!viewInterface || !viewInterface->tableInterface()) {
        qCritical() << "The QTableView accessibility interface is unavailable";
        return 2;
    }

    // Force creation and registration of an objectless QAccessibleTableCell.
    QAccessibleTableInterface *table = viewInterface->tableInterface();
    QAccessibleInterface *cell = table->cellAt(0, 0);
    if (!cell) {
        qCritical() << "Could not create the fictional cell interface";
        return 2;
    }
    g_targetId = QAccessible::uniqueId(cell);
    qInfo() << "table interface id:" << QAccessible::uniqueId(viewInterface)
            << " cell interface id:" << g_targetId;

    const auto previousHandler = QAccessible::installUpdateHandler(&onAccessibilityUpdate);

    QTimer::singleShot(0, &app, [&] {
        if (mode == QLatin1String("destroy")) {
            qInfo() << "destroying the fictional table view";
            delete view;
            view = nullptr;
            qInfo() << "view destruction returned";
        } else if (mode == QLatin1String("reset")) {
            qInfo() << "resetting the fictional table view";
            view->reset();
            qInfo() << "view reset returned";
        } else {
            qInfo() << "removing fictional row 0";
            model.removeRows(0, 1);
            qInfo() << "removeRows returned";
        }

        QAccessible::installUpdateHandler(previousHandler);

        const bool stillCached = QAccessible::accessibleInterface(g_targetId) != nullptr;
        qInfo() << "cell interface still in cache:" << stillCached;
        qInfo() << "interface destructor count observed:" << g_destructorCount;

        if (stillCached) {
            qCritical() << "FAIL: deleted table cell interface is still cached";
            app.exit(1);
            return;
        }

        qInfo() << "PASS: the cell was removed without a stale cache access";
        app.exit(0);
    });

    return app.exec();
}
